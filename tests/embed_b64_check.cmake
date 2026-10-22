# Textual checks on the resource TU generated with CMRC_BASE64=ON.
#
# Compilation alone does not catch every defect in the base64 generator: the
# cmrc.hpp include guard used to keep a literal @INPUT_FILE@ placeholder (the
# @VAR@ substitution in CMakeRC.cmake runs in a single pass), which silently
# made every __has_embed() probe report the file as missing instead of failing
# the build. Assert on the generated text so that regression is caught too.
#
# Required -D arguments:
#   SRC  absolute path to the generated intermediate .cpp

if(NOT DEFINED SRC)
    message(FATAL_ERROR "embed_b64_check: missing -D SRC")
endif()
if(NOT EXISTS "${SRC}")
    message(FATAL_ERROR "embed_b64_check: generated source not found: ${SRC}\n"
        "Build the embed_b64 target before running this test.")
endif()
file(READ "${SRC}" source)

# No @PLACEHOLDER@ may survive string(CONFIGURE).
if(source MATCHES "@[A-Za-z_][A-Za-z0-9_]*@")
    message(FATAL_ERROR "embed_b64_check: unsubstituted @PLACEHOLDER@ in ${SRC}:\n${source}")
endif()

# A preprocessing directive must start its own line.
if(source MATCHES "\\{[ \t]*#embed")
    message(FATAL_ERROR "embed_b64_check: #embed must start its own line in ${SRC}:\n${source}")
endif()
if(NOT source MATCHES "\n[ \t]*#embed \"")
    message(FATAL_ERROR "embed_b64_check: no #embed directive emitted in ${SRC}:\n${source}")
endif()

# The cmrc.hpp include guard must probe the real input file.
if(NOT source MATCHES "__has_embed\\(\"[^\"]*hello\\.txt\"\\)")
    message(FATAL_ERROR "embed_b64_check: __has_embed() does not reference the input file in ${SRC}:\n${source}")
endif()

message(STATUS "embed_b64_check: OK")
