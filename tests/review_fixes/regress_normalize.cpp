// Regression tests for fix #5 of the 2026-09-06 review of
// include/cmrc/cmrc.hpp: detail::normalize_path() previously only stripped
// leading/trailing duplicate '/' separators and did not handle '.' / '..'
// components or backslashes, so paths like "subdir_a/subdir_b/../../hello.txt"
// failed to resolve and Windows-style "a\\b" paths were left untranslated.
//
// White-box: the function lives in cmrc::detail, so we test it directly.
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
  using cmrc::detail::normalize_path;

  // Identity / already-clean paths
  CHECK(normalize_path("") == "");
  CHECK(normalize_path("hello.txt") == "hello.txt");
  CHECK(normalize_path("a/b/c.txt") == "a/b/c.txt");

  // Backslash translation ('\\' -> '/')
  CHECK(normalize_path("a\\b\\c.txt") == "a/b/c.txt");
  CHECK(normalize_path("C:\\foo\\bar") == "C:/foo/bar");

  // Leading and trailing slashes
  CHECK(normalize_path("/hello.txt") == "hello.txt");
  CHECK(normalize_path("hello.txt/") == "hello.txt");

  // Duplicate separators
  CHECK(normalize_path("//a///b/") == "a/b");

  // "." components are dropped
  CHECK(normalize_path("./a/./b") == "a/b");
  CHECK(normalize_path("hello.txt/.") == "hello.txt");

  // ".." collapses the previous component
  CHECK(normalize_path("a/b/../c") == "a/c");
  CHECK(normalize_path("a/b/../../c/.") == "c");

  // ".." at the root cannot climb above the root (embedded FS has no parent)
  CHECK(normalize_path("../a") == "a");
  CHECK(normalize_path("..") == "");
  CHECK(normalize_path("a/../../b") == "b");

  // Real-world shapes used by the black-box errc test
  CHECK(normalize_path("subdir_a/subdir_b/../../hello.txt") == "hello.txt");
  CHECK(normalize_path("subdir_a\\subdir_b\\file_a.txt") ==
        "subdir_a/subdir_b/file_a.txt");

  if (failures == 0) {
    std::puts("normalize_path: all checks passed");
  }
  return failures == 0 ? 0 : 1;
}