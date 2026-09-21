# ===========================================================================
# Binary network-isolation gate.
#
#   cmake -DBINARY=<dll> -DOBJDUMP=<objdump> -P tools/check_network_isolation.cmake
#
# Fails (FATAL_ERROR, non-zero exit -> build fails) when the inspected binary
# imports ANY network DLL or network API symbol. This is the enforceable half of
# the frozen Stage 3 architecture:
#
#     bangla_tsf.dll -> LOCAL ONLY (engine / unicode / personal learning / model)
#
# The TSF DLL must never be able to open a socket, so the check is done on the
# real PE import table after every link — not on the sources.
# ===========================================================================
if(NOT DEFINED BINARY)
    message(FATAL_ERROR "network_isolation_check: BINARY not set")
endif()
if(NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "network_isolation_check: binary not found: ${BINARY}")
endif()
if(NOT DEFINED OBJDUMP OR OBJDUMP STREQUAL "")
    set(OBJDUMP objdump)
endif()

execute_process(COMMAND "${OBJDUMP}" -p "${BINARY}"
                OUTPUT_VARIABLE dump RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "network_isolation_check: '${OBJDUMP} -p ${BINARY}' failed (${rc})")
endif()

# --- 1. forbidden DLLs in the import table ---------------------------------
set(forbidden_dlls
    wininet winhttp ws2_32 wsock32 wsock32n urlmon httpapi dnsapi
    iphlpapi secur32 schannel ncrypt crypt32
)

string(REGEX MATCHALL "DLL Name: [^\r\n]+" import_lines "${dump}")
set(violations "")
foreach(line IN LISTS import_lines)
    string(TOLOWER "${line}" low)
    foreach(dll IN LISTS forbidden_dlls)
        if(low MATCHES "${dll}\\.dll")
            list(APPEND violations "${line}")
        endif()
    endforeach()
endforeach()

# --- 2. forbidden API symbols ---------------------------------------------
set(forbidden_symbols
    WinHttp InternetOpen InternetConnect InternetReadFile InternetCrackUrl
    WSASocket WSAStartup socket connect send recv DnsQuery
    URLDownloadToFile HttpOpenRequest
)
string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]+" symbols "${dump}")
foreach(sym IN LISTS symbols)
    foreach(pat IN LISTS forbidden_symbols)
        if(sym STREQUAL pat)
            list(APPEND violations "symbol ${sym}")
        endif()
    endforeach()
endforeach()

if(violations)
    list(REMOVE_DUPLICATES violations)
    string(REPLACE ";" "\n    " pretty "${violations}")
    if(ALLOW_NETWORK)
        # Cloud suggestions are enabled on purpose (user decision 2026-09-14):
        # report loudly, but do not fail the build.
        message(WARNING
            "NETWORK USAGE in ${BINARY} (allowed by ALLOW_NETWORK=1):\n    ${pretty}\n"
            "bangla_tsf.dll is NOT network-free right now (cloud suggestions ON).")
        return()
    endif()
    message(FATAL_ERROR
        "NETWORK ISOLATION VIOLATION in ${BINARY}:\n    ${pretty}\n"
        "bangla_tsf.dll must stay network-free (Stage 3 architecture). "
        "Move network code out of the TSF process (likhi_sync.exe).")
endif()

string(REGEX MATCHALL "DLL Name: [^\r\n]+" all_imports "${dump}")
list(LENGTH all_imports n_imports)
message(STATUS "network isolation check: ${BINARY} (${n_imports} imported DLLs, none network)")
