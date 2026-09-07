function(cmrc_add_test)
    set(options WILL_FAIL)
    set(args NAME PASS_REGEX WHENCE PREFIX)
    set(list_args RESOURCES TEST_ARGV)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${args}" "${list_args}")

    if(DEFINED ARG_WHENCE)
        set(whence_arg WHENCE "${ARG_WHENCE}")
    endif()
    if(DEFINED ARG_PREFIX)
        set(prefix_arg PREFIX "${ARG_PREFIX}")
    endif()

    add_executable("${ARG_NAME}" "${ARG_NAME}.cpp")
    cmrc_add_resource_library(rc_${ARG_NAME} NAMESPACE "${ARG_NAME}" ${whence_arg} ${prefix_arg} ${ARG_RESOURCES})
    target_link_libraries("${ARG_NAME}" PRIVATE rc_${ARG_NAME})
    add_test("${ARG_NAME}" "${ARG_NAME}" ${ARG_TEST_ARGV})
    if(DEFINED ARG_PASS_REGEX)
        set_property(
            TEST "${ARG_NAME}"
            PROPERTY PASS_REGULAR_EXPRESSION "${ARG_PASS_REGEX}"
            )
    endif()
    if(ARG_UNPARSED_ARGUMENTS)
        message(WARNING "Invalid test arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    set_property(TEST "${ARG_NAME}" PROPERTY WILL_FAIL "${ARG_WILL_FAIL}")
endfunction()

cmrc_add_test(
    NAME simple
    PASS_REGEX "^Hello, world!"
    RESOURCES
        hello.txt
        # Resources not used by the test, but just make sure we get no compilation
        # errors with the more complex paths:
        subdir_a/subdir_b/file_a.txt
        subdir_a/subdir_b/file_b.txt
    )

cmrc_add_test(
    NAME flower
    RESOURCES flower.jpg
    TEST_ARGV "${CMAKE_CURRENT_SOURCE_DIR}/flower.jpg"
    )

# Build + run the same flower resource under the CMRC_BASE64 fallback and
# compare the two generated intermediate sources. flower.jpg (2.6 MB) is large
# enough that the base64 form (~1.33x source expansion, decoded at startup)
# shrinks the generated TU ~5x versus the '\xNN' literal form (~6x source
# expansion) — the actual win of the flag is faster compilation of the
# generated resource TU, not a smaller executable (the base64 exe is typically
# larger because it stores the encoded string AND makes a decoded heap copy).
# The b64 test reports the one-time startup (first-resolve) time and verifies
# the decoded bytes match the on-disk file byte-for-byte. The nested build
# also exercises CMRC_BASE64 end-to-end through the real CMakeRC module.
add_test(
    NAME flower_b64
    COMMAND ${CMAKE_COMMAND}
        -DCMRC_MODULE=${PROJECT_SOURCE_DIR}/CMakeRC.cmake
        -DGEN=${CMAKE_GENERATOR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/flower_b64_scratch
        -DFLOWER_SRC=${CMAKE_CURRENT_SOURCE_DIR}/flower.jpg
        -P ${CMAKE_CURRENT_SOURCE_DIR}/flower_b64/run.cmake
    )

cmrc_add_test(
    NAME prefix
    PASS_REGEX "^Hello, world!"
    RESOURCES hello.txt
    PREFIX some-prefix
    )

cmrc_add_test(
    NAME whence
    PASS_REGEX "^I am a file!"
    RESOURCES subdir_a/subdir_b/file_a.txt
    WHENCE subdir_a
    )

cmrc_add_test(
    NAME whence_prefix
    PASS_REGEX "^I am a file!"
    RESOURCES subdir_a/subdir_b/file_b.txt
    WHENCE subdir_a
    PREFIX imaginary-prefix/
    )

cmrc_add_test(
    NAME iterate
    PASS_REGEX "^subdir_a\nfile_a.txt\nfile_b.txt\n$"
    RESOURCES
        subdir_a/subdir_b/file_a.txt
        subdir_a/subdir_b/file_b.txt
    )

cmrc_add_test(
    NAME enoent
    WILL_FAIL
    )

# Regression test for https://github.com/KeyWorksRW/CMakeRC2/issues/17
# (CMP0058: cmrc/cmrc.hpp must be an explicit build-time product, not a
# configure-time file(GENERATE) side effect, or Ninja flags the resource
# loader's DEPENDS on it as an undeclared byproduct). The driver script
# configures and builds a tiny consumer project, deletes the generated
# header and resource-loader sources, then rebuilds: with the fix the header
# is regenerated and the build succeeds; without it the rebuild fails.
add_test(
    NAME header_regen
    COMMAND ${CMAKE_COMMAND}
        -DCMRC_MODULE=${PROJECT_SOURCE_DIR}/CMakeRC.cmake
        -DGEN=${CMAKE_GENERATOR}
        -DBIN_ROOT=${CMAKE_CURRENT_BINARY_DIR}/header_regen_scratch
        -P ${CMAKE_CURRENT_SOURCE_DIR}/header_regen/run.cmake
    )


# Regression tests for the 2026-09-06 code review of include/cmrc/cmrc.hpp
# (see .ttm/code-review-cmrc-hpp.md for the fixes under test).
add_subdirectory(review_fixes)
