# Purpose: Locates the local Pico SDK and delegates to the SDK-provided import
# helper used by the root firmware build.
# Interface: Consumed by CMake via include(pico_sdk_import.cmake); callers may
# pass PICO_SDK_PATH or define it in the environment to override auto-detection.
# Constraints: Defaults to the Pico VS Code installer SDK path on this machine
# and fails fast when no SDK path is available.
# Ownership: This wrapper owns only local SDK discovery; the imported helper is
# owned by the Pico SDK installation.
#
# The Pico VS Code installer places SDK 2.2.0 here on this machine. If a
# different SDK is desired, pass -DPICO_SDK_PATH=... or export PICO_SDK_PATH.
if (NOT PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
    set(_LOCAL_PICO_SDK "$ENV{HOME}/.pico-sdk/sdk/2.2.0")
    if (EXISTS "${_LOCAL_PICO_SDK}/external/pico_sdk_import.cmake")
        set(PICO_SDK_PATH "${_LOCAL_PICO_SDK}" CACHE PATH "Path to Pico SDK")
    endif()
endif()

if (NOT PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "Set PICO_SDK_PATH or install the Pico SDK under ~/.pico-sdk/sdk/2.2.0")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
