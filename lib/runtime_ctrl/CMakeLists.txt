#
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
#

add_library(rmm-lib-runtime_ctrl)

target_link_libraries(rmm-lib-runtime_ctrl
    PRIVATE rmm-lib-arch
            rmm-lib-debug
            rmm-lib-smc)

target_include_directories(rmm-lib-runtime_ctrl
    PUBLIC "include")

target_sources(rmm-lib-runtime_ctrl
    PRIVATE "src/runtime_ctrl.c")
