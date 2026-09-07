cmake_minimum_required(VERSION 3.12...4.0)
project(flower_b64 CXX)

# CMRC_MODULE is passed by the driver script.
include("${CMRC_MODULE}")

# The resource lives in the parent tests/ directory; use WHENCE so CMakeRC
# accepts a file outside the nested project's source dir.
set(FLOWER_JPG "${CMAKE_CURRENT_SOURCE_DIR}/../flower.jpg")

# CMRC_DISABLE_EMBED forces the real fallback path on #embed-capable compilers
# so the literal-vs-base64 comparison exercises the actual generators.
set(CMRC_DISABLE_EMBED ON)

# Literal fallback build.
cmrc_add_resource_library(rc_flower_literal NAMESPACE flower_literal
    WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${FLOWER_JPG}")
add_executable(flower_literal flower.cpp)
target_compile_definitions(flower_literal PRIVATE CMRC_TEST_NS=flower_literal)
target_link_libraries(flower_literal PRIVATE rc_flower_literal)

# Base64 fallback build.
set(CMRC_BASE64 ON)
cmrc_add_resource_library(rc_flower_b64 NAMESPACE flower_b64
    WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${FLOWER_JPG}")
add_executable(flower_b64 flower.cpp)
target_compile_definitions(flower_b64 PRIVATE CMRC_TEST_NS=flower_b64)
target_link_libraries(flower_b64 PRIVATE rc_flower_b64)
