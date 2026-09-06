// Regression test for fix #2 of the 2026-09-06 review of include/cmrc/cmrc.hpp:
// the CMRC_NO_EXCEPTIONS error branches in open()/iterate_directory() call
// fprintf(stderr, ...) and abort(); before <cstdio>/<cstdlib> were added to the
// header's include block, this translation unit failed to compile ("fprintf"
// and "abort" undeclared).
//
// CMRC_NO_EXCEPTIONS must be defined before including the header; the header
// honours a pre-defined macro. This TU therefore compiles the fprintf/abort
// branches (the regression this test guards) while executing only the success
// paths below — the abort paths cannot be run without killing the test process.
#define CMRC_NO_EXCEPTIONS 1
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
  using namespace cmrc;

  static const char payload[] = "noexcept";
  static detail::directory dir;
  static detail::file_or_directory dir_fod{dir};

  // Populate the directory's internal index exactly as the generated loader
  // does (via add_file): iterate_directory() walks this internal map, while
  // the outer index_type map holds the full-path -> entry pointers. The same
  // add_file() result is registered in both so open() and iterate agree.
  auto *fentry =
      dir.add_file("file.txt", payload, payload + sizeof(payload) - 1);

  static detail::index_type index;
  index.emplace("data/file.txt", fentry);
  index.emplace("data", &dir_fod);

  embedded_filesystem fs(index);

  CHECK(fs.exists("data/file.txt"));
  CHECK(fs.is_file("data/file.txt"));
  CHECK(fs.is_directory("data"));
  CHECK(!fs.is_file("data"));
  CHECK(!fs.is_directory("data/file.txt"));
  CHECK(!fs.exists("missing.txt"));

  auto f = fs.open("data/file.txt");
  CHECK(std::string(f.begin(), f.end()) == "noexcept");

  int entries = 0;
  for (auto &&entry : fs.iterate_directory("data")) {
    ++entries;
    CHECK(entry.filename() == "file.txt");
  }
  CHECK(entries == 1);

  // The CMRC_NO_EXCEPTIONS error branches (fprintf + abort) exercised for
  // compilation above; they cannot be executed here without terminating the
  // test process.
  if (failures == 0) {
    std::puts("regress_noexcept: all checks passed");
  }
  return failures == 0 ? 0 : 1;
}
