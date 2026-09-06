# Minimal consumer project used by the header_regen regression test.
#
# The point of this project is to verify that cmrc/cmrc.hpp is produced as a
# build-time product of a custom command (and thus regenerated when it is
# missing), rather than being an untracked configure-time side effect of
# CMakeRC.cmake. See run.cmake for how the test exercises that contract.
cmake_minimum_required(VERSION 3.12)
project(header_regen CXX)

# The test passes this in so the consumer uses the real CMakeRC from the
# source tree (see the add_test() registration in tests/CMakeLists.txt).
include("${CMRC_MODULE}")

cmrc_add_resource_library(rc_hello NAMESPACE header_regen resource.txt)

add_executable(header_regen main.cpp)
target_link_libraries(header_regen PRIVATE rc_hello)
