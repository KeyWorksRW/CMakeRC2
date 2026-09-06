// Regression test for fixes #5 and #6 of the 2026-09-06 review of
// include/cmrc/cmrc.hpp, exercised through the public API over a real
// generated resource library (see CMakeLists.txt in this directory):
//   #6 open() on an existing directory now reports std::errc::is_a_directory
//       (previously it reported no_such_file_or_directory, contradicting the
//       is_directory() predicate of the same entry).
//   #5 path normalization: '.', '..', backslash and duplicate-slash variants
//       resolve to the same embedded file as the canonical path.
//
// Embedded resources (WHENCE ".."): "hello.txt" and
// "subdir_a/subdir_b/file_a.txt".
#include <cmrc/cmrc.hpp>

#include <cstdio>
#include <string>
#include <system_error>

CMRC_DECLARE(regress_errc);

static int failures = 0;

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      std::printf("FAIL: %s (line %d)\n", #expr, __LINE__);                    \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static bool throws_errc(const cmrc::embedded_filesystem &fs,
                        const std::string &path, std::errc expected) {
  try {
    (void)fs.open(path);
  } catch (const std::system_error &e) {
    return e.code() == std::make_error_code(expected);
  } catch (...) {
    return false;
  }
  return false;
}

int main() {
  auto fs = cmrc::regress_errc::get_filesystem();

  // --- #6: opening an existing directory yields is_a_directory -------------
  CHECK(throws_errc(fs, "subdir_a", std::errc::is_a_directory));
  CHECK(throws_errc(fs, "subdir_a/subdir_b", std::errc::is_a_directory));

  // ...while a genuinely missing file still yields no_such_file_or_directory.
  CHECK(throws_errc(fs, "missing_file.txt",
                    std::errc::no_such_file_or_directory));

  // Sanity: opening an existing file still succeeds (exact content; the fixture
  // files contain no trailing newline).
  auto hello = fs.open("hello.txt");
  CHECK(std::string(hello.begin(), hello.end()) == "Hello, world!");

  // --- #5 via the public API: normalized variants resolve to the same file --
  auto direct = fs.open("subdir_a/subdir_b/file_a.txt");
  CHECK(std::string(direct.begin(), direct.end()) == "I am a file!");

  auto dotdot = fs.open("subdir_a/subdir_b/../../hello.txt");
  CHECK(std::string(dotdot.begin(), dotdot.end()) == "Hello, world!");

  auto dot = fs.open("./hello.txt");
  CHECK(std::string(dot.begin(), dot.end()) == "Hello, world!");

  auto backslash = fs.open("subdir_a\\subdir_b\\file_a.txt");
  CHECK(std::string(backslash.begin(), backslash.end()) == "I am a file!");

  auto dup_slash = fs.open("subdir_a//subdir_b///file_a.txt");
  CHECK(std::string(dup_slash.begin(), dup_slash.end()) == "I am a file!");

  // exists()/is_file()/is_directory() agree with open() on normalized paths.
  CHECK(fs.exists("subdir_a/../hello.txt"));
  CHECK(fs.is_file("subdir_a/../hello.txt"));
  CHECK(fs.is_directory("subdir_a/subdir_b/.."));
  CHECK(fs.exists("../hello.txt")); // ".." at the root is dropped (embedded FS
                                    // has no parent)
  CHECK(!fs.exists("../definitely_missing.txt"));

  if (failures == 0) {
    std::puts("regress_errc: all checks passed");
  }
  return failures == 0 ? 0 : 1;
}
