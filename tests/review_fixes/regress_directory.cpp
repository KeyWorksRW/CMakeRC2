// Regression tests for the container-iteration fixes from the 2026-09-06
// review of include/cmrc/cmrc.hpp:
//   #1 directory::end() used to return a default-constructed ("singular")
//      iterator; comparing against or incrementing towards it was undefined
//      behaviour. Iterating a detail::directory directly with the container's
//      begin()/end() produced no entries on the common standard libraries.
//   #7 directory::iterator gained operator-> (backed by a cached
//   directory_entry
//      that operator* fills; operator-> alone on a fresh iterator is out of
//      contract and intentionally not tested).
//   #4 add_file/add_subdir duplicate handling was hardened; normal flow must
//      keep files and subdirectories distinct and individually browsable.
#include <cmrc/cmrc.hpp>

#include <cstdio>
#include <string>

static int failures = 0;

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::printf("FAIL: %s (line %d)\n", #expr, __LINE__);                    \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

int main() {
  using cmrc::detail::directory;

  static const char data1[] = "one";
  static const char data2[] = "two";

  directory d;
  auto *f1 = d.add_file("one.txt", data1, data1 + 3);
  auto *f2 = d.add_file("two.txt", data2, data2 + 3);
  auto sub = d.add_subdir("sub");
  auto *f3 = sub.directory.add_file("three.txt", data1, data1 + 3);

  CHECK(f1 != nullptr && f2 != nullptr && f3 != nullptr);
  CHECK(f1->is_file() && !f1->is_directory());
  CHECK(f2->is_file());
  CHECK(f3->is_file());
  CHECK(sub.index_entry.is_directory() && !sub.index_entry.is_file());

  // --- #1: the container's end() must be a proper sentinel -----------------
  CHECK(d.begin() != d.end());

  int count = 0;
  for (auto it = d.begin(); it != d.end(); ++it) {
    ++count;
  }
  CHECK(count == 3); // one.txt, sub, two.txt

  // range-for over the container itself exercises begin()/end() + operator*()
  count = 0;
  for (auto &&entry : d) {
    ++count;
    bool ok = entry.filename() == "one.txt" || entry.filename() == "two.txt" ||
              entry.filename() == "sub";
    CHECK(ok);
    CHECK(entry.is_file() || entry.is_directory());
  }
  CHECK(count == 3);

  // The iterator's own end() (used by the public iterate_directory() range-for)
  auto cur = d.begin();
  auto sentinel = cur.end();
  int n = 0;
  while (cur != sentinel) {
    ++n;
    ++cur;
  }
  CHECK(n == 3);
  CHECK(cur == d.end());

  // end() comparisons
  CHECK(d.end() == d.end());
  CHECK(!(d.begin() == d.end()));

  // --- #7: operator-> returns a pointer to the cached entry ------------------
  // operator*() returns the cached entry BY VALUE, so the addressed object is
  // the one stored in the cache (returned by operator->), not the temporary
  // copy the caller receives; compare contents, not addresses.
  int c2 = 0;
  for (auto it = d.begin(); it != d.end(); ++it) {
    const cmrc::directory_entry r = *it; // fills the cache; value copy
    const cmrc::directory_entry *p = it.operator->();
    CHECK(p != nullptr);
    CHECK(p->filename() == r.filename());
    CHECK(p->is_file() == r.is_file());
    ++c2;
  }
  CHECK(c2 == 3);

  // post-increment still reaches the end
  int c3 = 0;
  for (auto it = d.begin(); it != d.end(); it++) {
    ++c3;
  }
  CHECK(c3 == 3);

  // --- #4: files and subdirectories coexist and are separately browsable -----
  int files = 0;
  int dirs = 0;
  for (auto &&entry : d) {
    if (entry.is_file()) {
      ++files;
    }
    if (entry.is_directory()) {
      ++dirs;
    }
  }
  CHECK(files == 2);
  CHECK(dirs == 1);

  // The subdirectory is reachable and holds exactly its own file.
  int sub_entries = 0;
  for (auto &&entry : sub.directory) {
    ++sub_entries;
    CHECK(entry.filename() == "three.txt");
  }
  CHECK(sub_entries == 1);

  if (failures == 0) {
    std::puts("regress_directory: all checks passed");
  }
  return failures == 0 ? 0 : 1;
}
