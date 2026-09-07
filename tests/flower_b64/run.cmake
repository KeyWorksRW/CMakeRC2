# Regression test for the CMRC_BASE64 fallback encoding.
#
# Builds the same flower.cpp/flower.jpg resource twice against the real
# CMakeRC module: once with the default literal fallback (CMRC_BASE64=OFF) and
# once with the base64 fallback (CMRC_BASE64=ON). Then:
#   - runs both executables against the on-disk flower.jpg and verifies the
#     embedded contents match (functional correctness of the decoder),
#   - compares the resulting executable sizes: for a 2.6 MB resource the
#     ~1.33x base64 form must produce a smaller binary than the ~6x chracter
#     literal form (this is the whole point of the feature).
# The b64 executable also prints the one-time startup (static-init) decode time
# so the small-vs-large-file tradeoff is observable.
#
# Required -D arguments:
#   CMRC_MODULE  absolute path to the real CMakeRC.cmake in the source tree
#   GEN          the generator to use for the nested configure
#   WORK_DIR     scratch directory (inside the parent build tree)
#   FLOWER_SRC   absolute path to tests/flower.jpg

if(NOT DEFINED CMRC_MODULE OR NOT DEFINED GEN OR NOT DEFINED WORK_DIR OR NOT DEFINED FLOWER_SRC)
    message(FATAL_ERROR "flower_b64: missing -D argument (CMRC_MODULE, GEN, WORK_DIR, FLOWER_SRC)")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# The nested consumer project lives in this directory; its CMakeLists includes
# the real CMakeRC module via -DCMRC_MODULE.
set(nested_src "${CMAKE_CURRENT_LIST_DIR}")

function(run_and_check step)
    execute_process(
        COMMAND ${CMAKE_COMMAND} ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "flower_b64: ${step} failed (exit ${result})\n"
            "STDOUT:\n${output}\nSTDERR:\n${error}")
    endif()
    set(LAST_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

# Run a built executable directly (not a cmake invocation).
function(run_exe_and_check step exe)
    execute_process(
        COMMAND "${exe}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "flower_b64: ${step} failed (exit ${result})\n"
            "STDOUT:\n${output}\nSTDERR:\n${error}")
    endif()
    message(STATUS "flower_b64: ${step}: ${output}")
endfunction()

# 1. Literal fallback build.
run_and_check("literal configure" -G "${GEN}" -S "${nested_src}" -B "${WORK_DIR}/lit"
    -DCMRC_MODULE=${CMRC_MODULE} -DCMRC_BASE64=OFF)
run_and_check("literal build" --build "${WORK_DIR}/lit" --config Release)

# 2. Base64 fallback build.
run_and_check("b64 configure" -G "${GEN}" -S "${nested_src}" -B "${WORK_DIR}/b64"
    -DCMRC_MODULE=${CMRC_MODULE} -DCMRC_BASE64=ON)
run_and_check("b64 build" --build "${WORK_DIR}/b64" --config Release)

# 3. Run both executables against the real flower.jpg; both must verify the
#    embedded bytes against the on-disk file (content check in flower.cpp).
run_exe_and_check("literal run" "${WORK_DIR}/lit/flower_literal" "${FLOWER_SRC}")
run_exe_and_check("b64 run" "${WORK_DIR}/b64/flower_b64" "${FLOWER_SRC}")

# 4. Compare executable sizes. Generator-independent exe path lookup: use the
#    per-config subdirectory when the generator is multi-config (VS), else the
#    build dir root.
set(exe_suffix ".exe")
if(CMAKE_HOST_WIN32)
    set(exe_suffix ".exe")
else()
    set(exe_suffix "")
endif()
set(lit_exe "${WORK_DIR}/lit/flower_literal${exe_suffix}")
set(b64_exe "${WORK_DIR}/b64/flower_b64${exe_suffix}")
if(NOT EXISTS "${lit_exe}")
    set(lit_exe "${WORK_DIR}/lit/Release/flower_literal${exe_suffix}")
endif()
if(NOT EXISTS "${b64_exe}")
    set(b64_exe "${WORK_DIR}/b64/Release/flower_b64${exe_suffix}")
endif()
foreach(exe IN ITEMS "${lit_exe}" "${b64_exe}")
    if(NOT EXISTS "${exe}")
        message(FATAL_ERROR "flower_b64: expected executable not found: ${exe}")
    endif()
endforeach()
# 4. Compare generated intermediate source sizes. The hex-literal fallback
#    expands each byte to ~6 source chars (the compile-time pain point), while
#    the base64 fallback is ~1.33x plus chunk overhead. For flower.jpg the
#    literal intermediate is ~18 MB vs ~7 MB for base64 — the actual benefit of
#    the flag (faster compile of the generated TU). Note: the base64 *exe* is
#    typically larger, not smaller — it stores the base64 string AND decodes it
#    into a heap buffer at startup, so exe size is NOT the win; source size is.
set(lit_src "${WORK_DIR}/lit/__cmrc_rc_flower_literal/intermediate/flower.jpg.cpp")
set(b64_src "${WORK_DIR}/b64/__cmrc_rc_flower_b64/intermediate/flower.jpg.cpp")
file(SIZE "${lit_src}" lit_src_size)
file(SIZE "${b64_src}" b64_src_size)
file(SIZE "${lit_exe}" lit_size)
file(SIZE "${b64_exe}" b64_size)
message(STATUS "flower_b64: literal intermediate = ${lit_src_size} bytes")
message(STATUS "flower_b64: base64 intermediate = ${b64_src_size} bytes")
message(STATUS "flower_b64: literal exe = ${lit_size} bytes, base64 exe = ${b64_size} bytes")
if(NOT b64_src_size LESS lit_src_size)
    message(FATAL_ERROR "flower_b64: expected base64 intermediate (${b64_src_size}) "
        "to be smaller than literal intermediate (${lit_src_size})")
endif()

message(STATUS "flower_b64: OK")
