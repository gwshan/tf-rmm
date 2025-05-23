#
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
#

add_library(rmm-app-attestation-stub)

target_link_libraries(rmm-app-attestation-stub
    rmm-el2-stub
    rmm-lib-arch
    rmm-lib-common
    rmm-lib-debug
    rmm-lib-rmm_el3_ifc
    rmm-lib-smc)

target_include_directories(rmm-app-attestation-stub
    PUBLIC "include"
    PRIVATE "../../common/include"
            "src")

target_compile_definitions(rmm-app-attestation-stub
    PUBLIC
        "ATTEST_EL3_TOKEN_SIGN=$<IF:$<BOOL:${ATTEST_EL3_TOKEN_SIGN}>,1,0>"
)

target_sources(rmm-app-attestation-stub
    PRIVATE
        "src/attest.c")

if(HOST_VARIANT STREQUAL "host_cbmc")
    target_sources(rmm-app-attestation-stub
        PRIVATE "src/fake_host/cbmc_rim.c")
else()
    target_sources(rmm-app-attestation-stub
        PRIVATE
            "src/measurement.c"
            "src/rim.c")

    if (ATTEST_EL3_TOKEN_SIGN)
        target_sources(rmm-app-attestation-stub
            PRIVATE
            "src/attestation_el3_token_sign.c")
    endif()
endif()

list(APPEND RMM_EL2_STUB_LIBRARIES "rmm-app-attestation-stub")
set(RMM_EL2_STUB_LIBRARIES ${RMM_EL2_STUB_LIBRARIES} PARENT_SCOPE)
