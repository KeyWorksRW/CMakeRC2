# Regression tests for the issues found and fixed in the 2026-09-06 code review
# of include/cmrc/cmrc.hpp (see .ttm/code-review-cmrc-hpp.md).
#
# Fixed problems under test:
#   #1  directory::end() returned a singular (default-constructed) iterator;
#       UB when iterating a detail::directory directly
#                                                      -> regress_directory.cpp
#   #2  fprintf()/abort() used without <cstdio>/<cstdlib> in the
#       CMRC_NO_EXCEPTIONS error paths               -> regress_noexcept.cpp
#   #4  add_file/add_subdir duplicate-name handling  -> regress_directory.cpp
#   #5  normalize_path() did not handle '.', '..', duplicate/trailing slashes
#       or backslashes -> regress_normalize.cpp + regress_errc.cpp (public API)
#   #6  open() on an existing directory reported no_such_file_or_directory
#       instead of is_a_directory                    -> regress_errc.cpp
#   #7  directory::iterator lacked operator->        -> regress_directory.cpp

# Header-only white-box tests: exercise cmrc::detail directly. They link
# cmrc::base only to pick up the build-tree include dir, and explicitly depend
# on cmrc-base-hdr so the generated copy of cmrc/cmrc.hpp exists before these
# translation units are compiled.
add_executable(regress_normalize regress_normalize.cpp)
target_link_libraries(regress_normalize PRIVATE cmrc::base)
add_dependencies(regress_normalize cmrc-base-hdr)
add_test(NAME regress_normalize COMMAND regress_normalize)

add_executable(regress_directory regress_directory.cpp)
target_link_libraries(regress_directory PRIVATE cmrc::base)
add_dependencies(regress_directory cmrc-base-hdr)
add_test(NAME regress_directory COMMAND regress_directory)

add_executable(regress_noexcept regress_noexcept.cpp)
target_link_libraries(regress_noexcept PRIVATE cmrc::base)
add_dependencies(regress_noexcept cmrc-base-hdr)
add_test(NAME regress_noexcept COMMAND regress_noexcept)

# Black-box test through the public embedded_filesystem API over a real
# generated resource library. Resources are taken from the tests/ folder
# (WHENCE ".."), so the embedded paths are hello.txt and
# subdir_a/subdir_b/file_a.txt.
cmrc_add_test(
    NAME regress_errc
    RESOURCES ../hello.txt ../subdir_a/subdir_b/file_a.txt
    WHENCE ..
    )