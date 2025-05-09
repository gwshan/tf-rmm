#
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
#

# Skip rmm-lib-spdm_requester from CMBC analysis
if(RMM_CBMC_STATIC_ANALYSIS)
    add_library(rmm-lib-spdm_requester INTERFACE)
    return()
endif()

set(LIBSPDM_SRC "${RMM_SOURCE_DIR}/ext/libspdm")

# Set LIBSPDM_DIR. Note: This variable is used by libspdm CMake.
# libspdm is fetched to ext/libspdm in RMM source.
set(LIBSPDM_DIR ${LIBSPDM_SRC})
set(LIBSPDM_BIN_DIR "${CMAKE_BINARY_DIR}/ext/libspdm")
set(MBEDTLS_SRC_DIR "${RMM_SOURCE_DIR}/ext/mbedtls")

# Apply RMM specific patches to libspdm once cloned.
set(LIBSPDM_PATCH_DIR "${RMM_SOURCE_DIR}/configs/libspdm")
set(LIBSPDM_PATCH_FILES
    "${LIBSPDM_PATCH_DIR}/0001-cryptlib_mbedtls-use-external-Mbed-TLS.patch"
    "${LIBSPDM_PATCH_DIR}/0002-fix-libspdm_hmac_new-return-and-LIBSPDM_STATUS_SUCCE.patch"
    )
Git_Apply_Patches(${LIBSPDM_DIR} "${LIBSPDM_PATCH_FILES}")

#
# This compiles libraries in ext/libspdm project to add SPDM requester protocol
# support in RMM.
#
add_library(rmm-lib-spdm_requester INTERFACE)

# Set libspdm config file
set(LIBSPDM_CONFIG "LIBSPDM_CONFIG=<rmm_libspdm_config.h>")

# Below are the list of core SPDM libraries required for requester side protocol.
set(SOURCE_DIRS
    ${LIBSPDM_DIR}/library/spdm_common_lib
    ${LIBSPDM_DIR}/library/spdm_crypt_lib
    ${LIBSPDM_DIR}/library/spdm_requester_lib
    ${LIBSPDM_DIR}/library/spdm_secured_message_lib)

#
# Below are the list of os_stub libraries in libspdm that LIBSPDM_CORE_LIBS
# depends on.
# TODO: memlib and debuglib should be reimplemented for aarch64.
#
set(SOURCE_DIRS
    ${SOURCE_DIRS}
    ${LIBSPDM_DIR}/os_stub/cryptlib_mbedtls
    ${LIBSPDM_DIR}/os_stub/memlib
    ${LIBSPDM_DIR}/os_stub/debuglib)

# For aarch64 build include aarch64_stub libraries that provides
# libspdm_get_random_number_64, libspdm_sleep else include libspdm's os_stub
# libraries.
if(RMM_ARCH STREQUAL aarch64)
    set(SOURCE_DIRS
        ${SOURCE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/aarch64_stub/spdm_malloclib
        ${CMAKE_CURRENT_SOURCE_DIR}/aarch64_stub/spdm_rnglib
        ${CMAKE_CURRENT_SOURCE_DIR}/aarch64_stub/spdm_timelib)
else()
    set(SOURCE_DIRS
        ${SOURCE_DIRS}
        ${LIBSPDM_DIR}/os_stub/malloclib
        ${LIBSPDM_DIR}/os_stub/rnglib
        ${LIBSPDM_DIR}/os_stub/platform_lib)
endif()

# Disable -Wstrict-overflow for libspdm
set(BACKUP_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE "-Wstrict-overflow" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

# Add all source directories
foreach(SOURCE_DIR IN LISTS SOURCE_DIRS)
    # Get the library basename
    get_filename_component(SPDM_LIB "${SOURCE_DIR}" NAME)

    # Add the source directory
    add_subdirectory(${SOURCE_DIR} ${LIBSPDM_BIN_DIR}/${SPDM_LIB})

    target_include_directories(${SPDM_LIB} PRIVATE
        "${RMM_SOURCE_DIR}/configs/libspdm"
        "${RMM_SOURCE_DIR}/configs/mbedtls"
        "${MBEDTLS_SRC_DIR}/include")
    target_compile_definitions(${SPDM_LIB} PRIVATE
        ${LIBSPDM_CONFIG}
        "-DMBEDTLS_CONFIG_FILE=<rmm_mbedtls_config.h>")

    # RMM default compile flags are passed to libspdm. Disable few warnings
    # reported as error until libspdm resolves these issues.
    target_compile_options(${SPDM_LIB} PRIVATE
        "-Wno-incompatible-pointer-types"
        "-Wno-unused-parameter"
        "-Wno-old-style-declaration"
        "-Wno-sign-compare")

    target_link_libraries(rmm-lib-spdm_requester INTERFACE ${SPDM_LIB})
endforeach()

set(CMAKE_C_FLAGS "${BACKUP_C_FLAGS}")

# Manually create dependencies for libspdm libraries
target_link_libraries(spdm_requester_lib PRIVATE spdm_common_lib spdm_crypt_lib)
target_link_libraries(spdm_secured_message_lib PRIVATE spdm_crypt_lib)
target_link_libraries(spdm_common_lib PRIVATE spdm_crypt_lib)
# Manually create dependencies for cryptlib_mbedtls library
target_link_libraries(cryptlib_mbedtls PRIVATE rmm-mbedtls)
