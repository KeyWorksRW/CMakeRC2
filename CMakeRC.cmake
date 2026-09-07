# This block is executed when generating an intermediate resource file, not when
# running in CMake configure mode
if(_CMRC_GENERATE_MODE)
    # Read in the digits
    file(READ "${INPUT_FILE}" bytes HEX)
    string(LENGTH "${bytes}" n_bytes2)
    math(EXPR n_bytes "${n_bytes2} / 2")
    if(CMRC_BASE64)
        # Encode the raw bytes as a standard base64 string. The hex string from
        # file(READ ... HEX) is processed in groups of 6 hex digits (= 3 bytes
        # = 4 base64 chars). math(EXPR) understands 0x... literals, so each
        # group is packed into an integer and split into four 6-bit indices;
        # the lookup table maps each index to its base64 character. A final
        # group of 2 or 4 hex digits (1 or 2 remaining bytes) produces 2 or 3
        # chars with '=' padding. This keeps the generated source at ~1.33x the
        # resource size instead of the ~6x of the '\xNN' literal fallback.
        set(_b64_alpha "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
        set(b64 "")
        string(LENGTH "${bytes}" _b64_hex_len)
        # Process the hex in slices. CMake expands a variable in full every time
        # it is referenced, so looping with "${bytes}" (the whole file's hex)
        # as the SUBSTRING source would be O(n^2). Instead each iteration pulls
        # a small slice once and encodes it with small local variables, then
        # appends the encoded slice to b64 in a single operation.
        set(_b64_slice_size 18000) # 9000 bytes per slice
        set(_b64_idx 0)
        while(_b64_idx LESS _b64_hex_len)
            string(SUBSTRING "${bytes}" ${_b64_idx} ${_b64_slice_size} _b64_slice_hex)
            string(LENGTH "${_b64_slice_hex}" _b64_slice_len)
            set(_b64_slice_b64 "")
            set(_b64_si 0)
            while(_b64_si LESS _b64_slice_len)
                string(SUBSTRING "${_b64_slice_hex}" ${_b64_si} 6 _b64_grp)
                string(LENGTH "${_b64_grp}" _b64_grp_len)
                if(_b64_grp_len EQUAL 6)
                    math(EXPR _b64_v "0x${_b64_grp}")
                    math(EXPR _b64_c1 "(${_b64_v} >> 18) & 63")
                    math(EXPR _b64_c2 "(${_b64_v} >> 12) & 63")
                    math(EXPR _b64_c3 "(${_b64_v} >> 6) & 63")
                    math(EXPR _b64_c4 "${_b64_v} & 63")
                    set(_b64_out_chars "")
                    foreach(_b64_ci IN ITEMS ${_b64_c1} ${_b64_c2} ${_b64_c3} ${_b64_c4})
                        string(SUBSTRING "${_b64_alpha}" ${_b64_ci} 1 _b64_ch)
                        set(_b64_out_chars "${_b64_out_chars}${_b64_ch}")
                    endforeach()
                    string(APPEND _b64_slice_b64 "${_b64_out_chars}")
                elseif(_b64_grp_len EQUAL 4)
                    # 2 remaining bytes -> 3 chars + '='
                    math(EXPR _b64_v "0x${_b64_grp}")
                    math(EXPR _b64_c1 "(${_b64_v} >> 10) & 63")
                    math(EXPR _b64_c2 "(${_b64_v} >> 4) & 63")
                    math(EXPR _b64_c3 "(${_b64_v} & 15) << 2")
                    set(_b64_out_chars "")
                    foreach(_b64_ci IN ITEMS ${_b64_c1} ${_b64_c2} ${_b64_c3})
                        string(SUBSTRING "${_b64_alpha}" ${_b64_ci} 1 _b64_ch)
                        set(_b64_out_chars "${_b64_out_chars}${_b64_ch}")
                    endforeach()
                    string(APPEND _b64_slice_b64 "${_b64_out_chars}=")
                elseif(_b64_grp_len EQUAL 2)
                    # 1 remaining byte -> 2 chars + '=='
                    math(EXPR _b64_v "0x${_b64_grp}")
                    math(EXPR _b64_c1 "(${_b64_v} >> 2) & 63")
                    math(EXPR _b64_c2 "(${_b64_v} & 3) << 4")
                    set(_b64_out_chars "")
                    foreach(_b64_ci IN ITEMS ${_b64_c1} ${_b64_c2})
                        string(SUBSTRING "${_b64_alpha}" ${_b64_ci} 1 _b64_ch)
                        set(_b64_out_chars "${_b64_out_chars}${_b64_ch}")
                    endforeach()
                    string(APPEND _b64_slice_b64 "${_b64_out_chars}==")
                endif()
                math(EXPR _b64_si "${_b64_si} + 6")
            endwhile()
            string(APPEND b64 "${_b64_slice_b64}")
            math(EXPR _b64_idx "${_b64_idx} + ${_b64_slice_size}")
        endwhile()
        # Split the base64 into chunks that stay under MSVC's per-literal and
        # post-concatenation caps (C2026 / 64K on pre-2022 compilers). Each
        # chunk is its own static array; b64_decode() stitches them back
        # together at runtime, so there is no single giant string literal
        # anywhere and arbitrarily large resources are supported on old MSVC.
        set(_b64_chunk_size 16000)
        set(_b64_chunks )
        set(_b64_idx 0)
        string(LENGTH "${b64}" _b64_blen)
        while(_b64_idx LESS _b64_blen)
            string(SUBSTRING "${b64}" ${_b64_idx} ${_b64_chunk_size} _b64_chunk)
            list(APPEND _b64_chunks "${_b64_chunk}")
            math(EXPR _b64_idx "${_b64_idx} + ${_b64_chunk_size}")
        endwhile()
        list(LENGTH _b64_chunks _b64_nchunks)
        # Build the C++ declarations for the chunk arrays, the length array
        # and the pointer array (substituted into the template below via @VAR@).
        set(_b64_emit "")
        set(_b64_lens_emit "static const std::size_t b64_lens[] = { ")
        set(_b64_parts_emit "static const char* const b64_parts[] = { ")
        set(_b64_ci 0)
        foreach(_b64_chunk IN LISTS _b64_chunks)
            if(NOT _b64_emit STREQUAL "")
                string(APPEND _b64_emit "\n")
            endif()
            string(APPEND _b64_emit "static const char b64_${_b64_ci}[] = \"${_b64_chunk}\";")
            string(APPEND _b64_lens_emit "sizeof(b64_${_b64_ci}) - 1, ")
            string(APPEND _b64_parts_emit "b64_${_b64_ci}, ")
            math(EXPR _b64_ci "${_b64_ci} + 1")
        endforeach()
        string(APPEND _b64_lens_emit "};")
        string(APPEND _b64_parts_emit "};")
    else()
        # Format each pair into a character literal. Heuristics seem to favor doing
        # the conversion in groups of five for fastest conversion
        string(REGEX REPLACE "(..)(..)(..)(..)(..)" "'\\\\x\\1','\\\\x\\2','\\\\x\\3','\\\\x\\4','\\\\x\\5'," chars "${bytes}")
        # Since we did this in groups, we have some leftovers to clean up
        math(EXPR remainder "${n_bytes} % 5") # <-- '5' is the grouping count from above
        set(cleanup_re "$")
        set(cleanup_sub )
        while(remainder)
            set(cleanup_re "(..)${cleanup_re}")
            set(cleanup_sub "'\\\\x\\${remainder}',${cleanup_sub}")
            math(EXPR remainder "${remainder} - 1")
        endwhile()
        if(NOT cleanup_re STREQUAL "$")
            string(REGEX REPLACE "${cleanup_re}" "${cleanup_sub}" chars "${chars}")
        endif()
    endif()
    # #embed takes a header-name token: normalize the resource path to forward
    # slashes and keep it quoted. The generated file lives in the build tree
    # while the resource lives in the source tree, so the absolute path is
    # baked into the directive; the #embed branch is only active when the
    # compiling compiler defines __has_embed AND reports the file as
    # embeddable, otherwise the hex-literal fallback below is emitted
    # unchanged (pre-#embed compilers, MSVC, etc. all take the fallback).
    file(TO_CMAKE_PATH "${INPUT_FILE}" INPUT_FILE)
    # The generated resource TU prefers #embed whenever __has_embed() reports
    # the file as embeddable. With CMRC_DISABLE_EMBED (diagnostic/testing only)
    # the guard is forced to #if 0 so the fallback path is compiled regardless.
    if(CMRC_DISABLE_EMBED)
        set(_cmrc_embed_guard "#if 0")
        # When the fallback is forced, the base64/literal code is always
        # emitted, so cmrc.hpp must always be included in base64 mode.
        set(_cmrc_b64_include_guard "#if 1")
    else()
        set(_cmrc_embed_guard "#if defined(__has_embed)")
        # Include cmrc.hpp (and enable the decoder) only when the code below
        # actually takes the base64 fallback (no #embed, or file not
        # embeddable). A #embed-capable compiler that can embed the file skips
        # the include entirely, so the decoder is not even compiled.
        set(_cmrc_b64_include_guard "#if !defined(__has_embed) || !__has_embed(\"@INPUT_FILE@\")")
    endif()
    if(n_bytes EQUAL 0)
        # A #embed of an empty file (without if_empty()) is ill-formed in some
        # compilers; keep the pre-existing zero-byte-array behaviour.
        string(CONFIGURE [[
            namespace { const char file_array[] = { 0 }; }
            namespace cmrc { namespace @NAMESPACE@ { namespace res_chars {
            extern const char* const @SYMBOL@_begin = file_array;
            extern const char* const @SYMBOL@_end = file_array + 0;
            }}}
        ]] code)
    else()
        if(CMRC_BASE64)
            string(CONFIGURE [[
            @_cmrc_b64_include_guard@
            #define CMRC_CMRC_HPP_BASE64
            #include <cmrc/cmrc.hpp>
            #endif
            namespace {
            @_cmrc_embed_guard@
            #  if __has_embed("@INPUT_FILE@")
            const char file_array[] = { #embed "@INPUT_FILE@" };
            const char* const file_ptr = file_array;
            #  else
            @_b64_emit@
            @_b64_lens_emit@
            @_b64_parts_emit@
            static const std::string b64_decoded = cmrc::detail::b64_decode(b64_parts, b64_lens, @_b64_nchunks@);
            const char* const file_ptr = b64_decoded.data();
            #  endif
            #else
            @_b64_emit@
            @_b64_lens_emit@
            @_b64_parts_emit@
            static const std::string b64_decoded = cmrc::detail::b64_decode(b64_parts, b64_lens, @_b64_nchunks@);
            const char* const file_ptr = b64_decoded.data();
            #endif
            }
            namespace cmrc { namespace @NAMESPACE@ { namespace res_chars {
            extern const char* const @SYMBOL@_begin = file_ptr;
            extern const char* const @SYMBOL@_end = file_ptr + @n_bytes@;
            }}}
        ]] code)
        else()
            string(CONFIGURE [[
            namespace { const char file_array[] = {
            @_cmrc_embed_guard@
            #  if __has_embed("@INPUT_FILE@")
            #embed "@INPUT_FILE@"
            #  else
            @chars@ 0
            #  endif
            #else
            @chars@ 0
            #endif
            }; }
            namespace cmrc { namespace @NAMESPACE@ { namespace res_chars {
            extern const char* const @SYMBOL@_begin = file_array;
            extern const char* const @SYMBOL@_end = file_array + @n_bytes@;
            }}}
        ]] code)
        endif()
    endif()
    file(WRITE "${OUTPUT_FILE}" "${code}")
    # Exit from the script. Nothing else needs to be processed
    return()
endif()

set(_version 3.0.0)

# Choose the fallback encoding used when #embed is not available: OFF (default)
# stores resources as '\xNN' character literals (~6x source expansion, zero
# runtime cost); ON stores them as base64 strings (~1.33x source expansion, a
# one-time decode at static-initialization time). Modern compilers get #embed
# regardless of this flag; it only affects the fallback path.
option(CMRC_BASE64 "Store resources as base64 (decoded at startup) when #embed is unavailable" OFF)

# Diagnostic/testing option: force the fallback path even on compilers that
# support #embed, so the literal/base64 generators can be exercised directly.
# Normally the generated resource TU prefers #embed whenever
# __has_embed() reports the file as embeddable, and this option is OFF.
option(CMRC_DISABLE_EMBED "Force the fallback encoding instead of #embed (diagnostic/testing only)" OFF)

cmake_minimum_required(VERSION 3.12...4.0)
include(CMakeParseArguments)

# CMake 3.31+ accepts a CODEGEN keyword in add_custom_command() which adds the
# generated files to the builtin `codegen` target, so lint/CI jobs can generate
# resources without compiling them. The keyword requires policy CMP0171 to be
# NEW. The 3.12...4.0 ceiling above already sets it on 3.31+ (the policy is
# introduced in 3.31), but this also covers consumers who include this module
# from a project whose own policy version is older. On CMake < 3.31 the policy
# does not exist, so this block is a no-op and the module still works with the
# 3.12 minimum.
if(POLICY CMP0171)
    cmake_policy(SET CMP0171 NEW)
endif()

if(COMMAND cmrc_add_resource_library)
    if(NOT DEFINED _CMRC_VERSION OR NOT (_version STREQUAL _CMRC_VERSION))
        message(WARNING "More than one CMakeRC version has been included in this project.")
    endif()
    # CMakeRC has already been included! Don't do anything
    return()
endif()

set(_CMRC_VERSION "${_version}" CACHE INTERNAL "CMakeRC version. Used for checking for conflicts")

set(_CMRC_SCRIPT "${CMAKE_CURRENT_LIST_FILE}" CACHE INTERNAL "Path to CMakeRC script")
# Top-level capture of the module's source directory. Functions cannot use
# CMAKE_CURRENT_LIST_DIR (it re-resolves to the caller's file), so file(READ)
# and friends inside the API functions must resolve the helper sources
# relative to this.

# Cached INTERNAL so the path is global like _CMRC_SCRIPT: when included from
# a FetchContent subdirectory, API functions called from the parent project
# must still see the module's directory (directory scopes inherit downward
# only).
set(_CMRC_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Path to CMakeRC module directory")

function(_cmrc_normalize_path var)
    set(path "${${var}}")
    file(TO_CMAKE_PATH "${path}" path)
    while(path MATCHES "//")
        string(REPLACE "//" "/" path "${path}")
    endwhile()
    string(REGEX REPLACE "/+$" "" path "${path}")
    set("${var}" "${path}" PARENT_SCOPE)
endfunction()

get_filename_component(_inc_dir "${CMAKE_BINARY_DIR}/_cmrc/include" ABSOLUTE)
set(CMRC_INCLUDE_DIR "${_inc_dir}" CACHE INTERNAL "Directory for CMakeRC include files")
# This module copies cmrc.hpp into the build tree; the copy is a declared
# build-time product (custom command + custom target) instead of a bare
# configure-time file(GENERATE). With the Ninja generator, a build-tree file
# that is listed in DEPENDS but is not the OUTPUT/BYPRODUCTS of any command
# triggers the CMP0058 developer warning copy_if_different preserves
# the old behavior of only touching the header when its content changes, so
# incremental builds stay clean.
set(_cmrc_src_hpp "${_CMRC_MODULE_DIR}/include/cmrc/cmrc.hpp")
set(cmrc_hpp "${CMRC_INCLUDE_DIR}/cmrc/cmrc.hpp" CACHE INTERNAL "")
add_custom_command(
    OUTPUT "${cmrc_hpp}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_cmrc_src_hpp}" "${cmrc_hpp}"
    COMMENT "Generating cmrc.hpp"
    DEPENDS "${_cmrc_src_hpp}"
    )
add_custom_target(cmrc-base-hdr ALL DEPENDS "${cmrc_hpp}")

add_library(cmrc-base INTERFACE)
target_include_directories(cmrc-base INTERFACE $<BUILD_INTERFACE:${CMRC_INCLUDE_DIR}>)
# Signal a basic C++11 feature to require C++11.
target_compile_features(cmrc-base INTERFACE cxx_nullptr)
set_property(TARGET cmrc-base PROPERTY INTERFACE_CXX_EXTENSIONS OFF)
add_library(cmrc::base ALIAS cmrc-base)

function(cmrc_add_resource_library name)
    set(args ALIAS NAMESPACE TYPE)
    cmake_parse_arguments(ARG "" "${args}" "" "${ARGN}")
    # Generate the identifier for the resource library's namespace
    set(ns_re "[a-zA-Z_][a-zA-Z0-9_]*")
    if(NOT DEFINED ARG_NAMESPACE)
        # Check that the library name is also a valid namespace
        if(NOT name MATCHES "${ns_re}")
            message(SEND_ERROR "Library name is not a valid namespace. Specify the NAMESPACE argument")
        endif()
        set(ARG_NAMESPACE "${name}")
    else()
        if(NOT ARG_NAMESPACE MATCHES "${ns_re}")
            message(SEND_ERROR "NAMESPACE for ${name} is not a valid C++ namespace identifier (${ARG_NAMESPACE})")
        endif()
    endif()
    set(libname "${name}")
    # Check that type is either "STATIC" or "OBJECT", or default to "STATIC" if
    # not set
    if(NOT DEFINED ARG_TYPE)
        set(ARG_TYPE STATIC)
    elseif(NOT "${ARG_TYPE}" MATCHES "^(STATIC|OBJECT)$")
        message(SEND_ERROR "${ARG_TYPE} is not a valid TYPE (STATIC and OBJECT are acceptable)")
        set(ARG_TYPE STATIC)
    endif()
    # Generate a library with the compiled in character arrays. The loader
    # template lives in cmake/cmrc_lib.cpp.in so it can be
    # reviewed and edited directly. It still goes through string(CONFIGURE @ONLY)
    # because it references target properties via generator expressions that can
    # only be evaluated at generate time by file(GENERATE).
    file(READ "${_CMRC_MODULE_DIR}/cmake/cmrc_lib.cpp.in" _cmrc_lib_tmpl)
    string(CONFIGURE "${_cmrc_lib_tmpl}" cpp_content @ONLY)

    get_filename_component(libdir "${CMAKE_CURRENT_BINARY_DIR}/__cmrc_${name}" ABSOLUTE)
    get_filename_component(lib_tmp_cpp "${libdir}/lib_.cpp" ABSOLUTE)
    string(REPLACE "\n        " "\n" cpp_content "${cpp_content}")
    file(GENERATE OUTPUT "${lib_tmp_cpp}" CONTENT "${cpp_content}")
    get_filename_component(libcpp "${libdir}/lib.cpp" ABSOLUTE)
    # CODEGEN is only valid when the policy exists; expand to nothing on older
    # CMake so add_custom_command() accepts it on 3.31+ and ignores it below.
    if(POLICY CMP0171)
        set(maybe_CODEGEN CODEGEN)
    else()
        set(maybe_CODEGEN "")
    endif()
    add_custom_command(OUTPUT "${libcpp}"
        DEPENDS "${lib_tmp_cpp}" "${cmrc_hpp}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${lib_tmp_cpp}" "${libcpp}"
        COMMENT "Generating ${name} resource loader"
        ${maybe_CODEGEN}
        )
    # Generate the actual static library. Each source file is just a single file
    # with a character array compiled in containing the contents of the
    # corresponding resource file.
    add_library(${name} ${ARG_TYPE} ${libcpp})
    # Ensure the generated cmrc/cmrc.hpp header exists before this library
    # (and anything that links it) is compiled.
    add_dependencies(${name} cmrc-base-hdr)
    set_property(TARGET ${name} PROPERTY CMRC_LIBDIR "${libdir}")
    set_property(TARGET ${name} PROPERTY CMRC_NAMESPACE "${ARG_NAMESPACE}")
    target_link_libraries(${name} PUBLIC cmrc::base)
    set_property(TARGET ${name} PROPERTY CMRC_IS_RESOURCE_LIBRARY TRUE)
    if(ARG_ALIAS)
        add_library("${ARG_ALIAS}" ALIAS ${name})
    endif()
    cmrc_add_resources(${name} ${ARG_UNPARSED_ARGUMENTS})
endfunction()

function(_cmrc_register_dirs name dirpath)
    if(dirpath STREQUAL "")
        return()
    endif()
    # Skip this dir if we have already registered it
    get_target_property(registered "${name}" _CMRC_REGISTERED_DIRS)
    if(dirpath IN_LIST registered)
        return()
    endif()
    # Register the parent directory first
    get_filename_component(parent "${dirpath}" DIRECTORY)
    if(NOT parent STREQUAL "")
        _cmrc_register_dirs("${name}" "${parent}")
    endif()
    # Now generate the registration
    set_property(TARGET "${name}" APPEND PROPERTY _CMRC_REGISTERED_DIRS "${dirpath}")
    _cm_encode_fpath(sym "${dirpath}")
    if(parent STREQUAL "")
        set(parent_sym root_directory)
    else()
        _cm_encode_fpath(parent_sym "${parent}")
    endif()
    get_filename_component(leaf "${dirpath}" NAME)
    set_property(
        TARGET "${name}"
        APPEND PROPERTY CMRC_MAKE_DIRS
        "static auto ${sym}_dir = ${parent_sym}_dir.directory.add_subdir(\"${leaf}\")\;"
        "root_index.emplace(\"${dirpath}\", &${sym}_dir.index_entry)\;"
        )
endfunction()

function(cmrc_add_resources name)
    get_target_property(is_reslib ${name} CMRC_IS_RESOURCE_LIBRARY)
    if(NOT TARGET ${name} OR NOT is_reslib)
        message(SEND_ERROR "cmrc_add_resources called on target '${name}' which is not an existing resource library")
        return()
    endif()

    set(options)
    set(args WHENCE PREFIX)
    set(list_args)
    cmake_parse_arguments(ARG "${options}" "${args}" "${list_args}" "${ARGN}")

    if(NOT ARG_WHENCE)
        set(ARG_WHENCE ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    _cmrc_normalize_path(ARG_WHENCE)
    get_filename_component(ARG_WHENCE "${ARG_WHENCE}" ABSOLUTE)

    # Generate the identifier for the resource library's namespace
    get_target_property(lib_ns "${name}" CMRC_NAMESPACE)

    get_target_property(libdir ${name} CMRC_LIBDIR)
    get_target_property(target_dir ${name} SOURCE_DIR)
    file(RELATIVE_PATH reldir "${target_dir}" "${CMAKE_CURRENT_SOURCE_DIR}")
    if(reldir MATCHES "^\\.\\.")
        message(SEND_ERROR "Cannot call cmrc_add_resources in a parent directory from the resource library target")
        return()
    endif()

    foreach(input IN LISTS ARG_UNPARSED_ARGUMENTS)
        _cmrc_normalize_path(input)
        get_filename_component(abs_in "${input}" ABSOLUTE)
        # Generate a filename based on the input filename that we can put in
        # the intermediate directory.
        file(RELATIVE_PATH relpath "${ARG_WHENCE}" "${abs_in}")
        if(relpath MATCHES "^\\.\\.")
            # For now we just error on files that exist outside of the source dir.
            message(SEND_ERROR "Cannot add file '${input}': File must be in a subdirectory of ${ARG_WHENCE}")
            continue()
        endif()
        if(DEFINED ARG_PREFIX)
            _cmrc_normalize_path(ARG_PREFIX)
        endif()
        if(ARG_PREFIX AND NOT ARG_PREFIX MATCHES "/$")
            set(ARG_PREFIX "${ARG_PREFIX}/")
        endif()
        get_filename_component(dirpath "${ARG_PREFIX}${relpath}" DIRECTORY)
        _cmrc_register_dirs("${name}" "${dirpath}")
        get_filename_component(abs_out "${libdir}/intermediate/${ARG_PREFIX}${relpath}.cpp" ABSOLUTE)
        # Generate a symbol name relpath the file's character array
        _cm_encode_fpath(sym "${relpath}")
        # Get the symbol name for the parent directory
        if(dirpath STREQUAL "")
            set(parent_sym root_directory)
        else()
            _cm_encode_fpath(parent_sym "${dirpath}")
        endif()
        # Generate the rule for the intermediate source file
        _cmrc_generate_intermediate_cpp(${lib_ns} ${sym} "${abs_out}" "${abs_in}")
        target_sources(${name} PRIVATE "${abs_out}")
        set_property(TARGET ${name} APPEND PROPERTY CMRC_EXTERN_DECLS
            "// Pointers to ${input}"
            "extern const char* const ${sym}_begin\;"
            "extern const char* const ${sym}_end\;"
            )
        get_filename_component(leaf "${relpath}" NAME)
        set_property(
            TARGET ${name}
            APPEND PROPERTY CMRC_MAKE_FILES
            "root_index.emplace("
            "    \"${ARG_PREFIX}${relpath}\","
            "    ${parent_sym}_dir.directory.add_file("
            "        \"${leaf}\","
            "        res_chars::${sym}_begin,"
            "        res_chars::${sym}_end"
            "    )"
            ")\;"
            )
    endforeach()
endfunction()

function(_cmrc_generate_intermediate_cpp lib_ns symbol outfile infile)
    if(POLICY CMP0171)
        set(maybe_CODEGEN CODEGEN)
    else()
        set(maybe_CODEGEN "")
    endif()
    add_custom_command(
        # This is the file we will generate
        OUTPUT "${outfile}"
        # These are the primary files that affect the output
        DEPENDS "${infile}" "${_CMRC_SCRIPT}"
        COMMAND
            "${CMAKE_COMMAND}"
                -D_CMRC_GENERATE_MODE=TRUE
                -DNAMESPACE=${lib_ns}
                -DSYMBOL=${symbol}
                "-DINPUT_FILE=${infile}"
                "-DOUTPUT_FILE=${outfile}"
                "-DCMRC_BASE64=${CMRC_BASE64}"
                "-DCMRC_DISABLE_EMBED=${CMRC_DISABLE_EMBED}"
                -P "${_CMRC_SCRIPT}"
        COMMENT "Generating intermediate file for ${infile}"
        ${maybe_CODEGEN}
    )
endfunction()

function(_cm_encode_fpath var fpath)
    string(MAKE_C_IDENTIFIER "${fpath}" ident)
    string(MD5 hash "${fpath}")
    string(SUBSTRING "${hash}" 0 4 hash)
    set(${var} f_${hash}_${ident} PARENT_SCOPE)
endfunction()
