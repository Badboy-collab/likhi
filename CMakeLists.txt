cmake_minimum_required(VERSION 3.20)
project(PC_Bangla_Typing_App VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(
    engine/include
    engine/src
    tsf/include
)

# Core Engine Source Files
set(ENGINE_SOURCES
    engine/src/unicode/bangla_unicode.cpp
    engine/src/transliteration/phonetic_parser.cpp
    engine/src/transliteration/fuzzy_normalizer.cpp
    engine/src/transliteration/bangla_morphology.cpp
    engine/src/hybrid/hybrid_brain.cpp
    engine/src/dictionary/lexicon_trie.cpp
    engine/src/ranking/context_ranker.cpp
    engine/src/personal_dict/personal_dictionary.cpp
    engine/src/bangla_engine.cpp
)

# Static Library Target
add_library(bangla_engine_static STATIC ${ENGINE_SOURCES})

# Custom Windres Resource Commands
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/settings_rc.obj
    COMMAND windres -i "${CMAKE_CURRENT_SOURCE_DIR}/tools/settings_app/settings.rc" -o "${CMAKE_CURRENT_BINARY_DIR}/settings_rc.obj"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/settings_app/settings.rc" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
    COMMENT "Compiling Settings Resource"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tsf_rc.obj
    COMMAND windres -i "${CMAKE_CURRENT_SOURCE_DIR}/tsf/src/tsf.rc" -o "${CMAKE_CURRENT_BINARY_DIR}/tsf_rc.obj"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tsf/src/tsf.rc" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
    COMMENT "Compiling TSF Resource"
)

# TSF Framework Source Files
#
# Network boundary (decision 2026-09-14): cloud_translit.cpp IS linked in on
# purpose, so bangla_tsf.dll imports WINHTTP.dll and serves the online
# suggestions the user asked for. The network_isolation_check gate below runs
# in WARNING mode and prints every network import on each link, so the IME can
# be re-isolated later (likhi_sync.exe + filesystem staging) by building with
# -DLIKHI_ALLOW_NETWORK=0.
# TSF Framework Source Files
set(TSF_SOURCES
    tsf/src/dll_main.cpp
    tsf/src/class_factory.cpp
    tsf/src/text_service.cpp
    tsf/src/key_event_sink.cpp
    tsf/src/key_policy.cpp
    tsf/src/composition_mgr.cpp
    tsf/src/edit_session.cpp
    tsf/src/candidate_window.cpp
    tsf/src/cloud_translit.cpp
    tsf/src/voice_input.cpp    # online speech->text (user-approved, same boundary)
    tsf/src/register.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/tsf_rc.obj
    tsf/src/bangla_tsf.def
)

# Windows TSF DLL Target
add_library(bangla_tsf SHARED ${TSF_SOURCES})
set_target_properties(bangla_tsf PROPERTIES
    PREFIX ""
    OUTPUT_NAME "bangla_tsf"
)
target_link_libraries(bangla_tsf PRIVATE
    bangla_engine_static
    ole32
    oleaut32
    user32
    gdi32
    comctl32
    advapi32
    shell32
    winhttp
    winmm      # voice input capture (waveIn)
)

# ---- Network boundary gate (warning while cloud suggestions are ON) --------
# Runs after EVERY link of the IME DLL and prints its network imports, so the
# boundary stays visible. -DLIKHI_ALLOW_NETWORK=0 makes it fatal again.
set(LIKHI_ALLOW_NETWORK 1 CACHE STRING "1 = cloud suggestions allowed in bangla_tsf.dll (warn), 0 = enforce network-free (fatal)")
get_filename_component(_likhi_toolchain_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(LIKHI_OBJDUMP NAMES objdump HINTS "${_likhi_toolchain_bin}")
if(LIKHI_OBJDUMP)
    add_custom_command(TARGET bangla_tsf POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DBINARY=$<TARGET_FILE:bangla_tsf>
                -DOBJDUMP="${LIKHI_OBJDUMP}"
                -DALLOW_NETWORK=${LIKHI_ALLOW_NETWORK}
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_network_isolation.cmake"
        COMMENT "network isolation gate: bangla_tsf.dll import table"
    )
else()
    message(WARNING "objdump not found - bangla_tsf.dll network isolation gate DISABLED")
endif()

# Interactive CLI Tool Target
add_executable(cli_engine tools/cli_engine.cpp)
target_link_libraries(cli_engine PRIVATE bangla_engine_static)

# Unit Test Runner Target
add_executable(test_runner tests/unit/test_runner.cpp)
target_link_libraries(test_runner PRIVATE bangla_engine_static)

# Unicode Integrity Test Target
add_executable(unicode_bengali_tests tests/unit/unicode_bengali_tests.cpp)
target_link_libraries(unicode_bengali_tests PRIVATE bangla_engine_static)

# Personal Learning Engine Test Target (usage ranking + file persistence)
add_executable(test_learning tests/unit/test_learning.cpp)
target_link_libraries(test_learning PRIVATE bangla_engine_static)

# Morphology Regression Test Target (Release Blocker 1)
add_executable(test_morphology tests/unit/test_morphology.cpp)
target_link_libraries(test_morphology PRIVATE bangla_engine_static)

# Hybrid Suggestion Intelligence Test Target
add_executable(test_hybrid_brain tests/unit/test_hybrid_brain.cpp)
target_link_libraries(test_hybrid_brain PRIVATE bangla_engine_static)

# Suggestion Popup Quality Acceptance Suite
add_executable(test_popup_quality tests/test_popup_quality.cpp)
target_link_libraries(test_popup_quality PRIVATE bangla_engine_static)

# Final Acceptance Verification Runner (38-test acceptance suite)
add_executable(acceptance_verification_runner tools/acceptance_verification_runner.cpp)
target_link_libraries(acceptance_verification_runner PRIVATE bangla_engine_static)

# TSF Integration Test Target
add_executable(test_tsf_integration
    tests/tsf_integration/test_tsf_integration.cpp
    tsf/src/composition_mgr.cpp
    tsf/src/candidate_window.cpp
    tsf/src/edit_session.cpp
    tsf/src/cloud_translit.cpp
    tsf/src/voice_input.cpp
)
target_link_libraries(test_tsf_integration PRIVATE
    winmm
    bangla_engine_static
    ole32
    oleaut32
    user32
    gdi32
    comctl32
    advapi32
    shell32
    winhttp
)

# P0 Critical Keyboard Event Test Target
add_executable(test_p0_keyboard
    tests/unit/test_p0_keyboard.cpp
    tsf/src/key_event_sink.cpp
    tsf/src/key_policy.cpp
    tsf/src/composition_mgr.cpp
    tsf/src/candidate_window.cpp
    tsf/src/edit_session.cpp
    tsf/src/cloud_translit.cpp
    tsf/src/voice_input.cpp
    tsf/src/text_service.cpp
)
target_link_libraries(test_p0_keyboard PRIVATE
    winmm
    bangla_engine_static
    ole32
    oleaut32
    user32
    gdi32
    comctl32
    advapi32
    shell32
    winhttp
)

# Key Policy Regression Test Target (pure, no TSF runtime needed)
add_executable(test_key_policy
    tests/unit/test_key_policy.cpp
    tsf/src/key_policy.cpp
)
target_link_libraries(test_key_policy PRIVATE user32)

# Universal Mode (non-TSF fallback input path): pure decision layer, shared by
# the Win32 host. No Windows types, so it is testable without a desktop session.
add_library(likhi_universal_core STATIC universal/src/universal_typing.cpp)
target_include_directories(likhi_universal_core PUBLIC universal/include)

# Universal Mode safety regression target
add_executable(test_universal_typing tests/unit/test_universal_typing.cpp)
target_link_libraries(test_universal_typing PRIVATE likhi_universal_core)

# Update Service Target (WinHTTP + CryptoAPI + Semantic Versioning)
add_library(likhi_update_service STATIC
    engine/src/update/update_service.cpp
)
target_include_directories(likhi_update_service PUBLIC engine/include)
target_link_libraries(likhi_update_service PUBLIC
    winhttp
    crypt32
    shell32
    ole32
)

# Automated Update Service Test Target
add_executable(test_update_service
    tests/unit/test_update_service.cpp
)
target_link_libraries(test_update_service PRIVATE
    likhi_update_service
)

# Universal Mode host (non-TSF fallback input path): tray icon + low-level
# keyboard hook that types Bangla through Unicode keystrokes where TSF cannot
# reach (UWP/Store apps, Java UIs, games, remote sessions).
add_executable(likhi_universal WIN32
    tools/universal/likhi_universal.cpp
)
# UNICODE/_UNICODE: IDI_APPLICATION and friends must resolve to the wide variants.
target_compile_definitions(likhi_universal PRIVATE UNICODE _UNICODE)
target_link_libraries(likhi_universal PRIVATE
    likhi_universal_core
    bangla_engine_static
    likhi_update_service
    user32
    gdi32
    shell32
    advapi32
    psapi
)

# Native Settings App Target
add_executable(bangla_settings WIN32
    tools/settings_app/settings_main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/settings_rc.obj
)
target_link_libraries(bangla_settings PRIVATE
    likhi_update_service
    user32
    gdi32
    comctl32
    shell32
    uxtheme
    dwmapi
)

# One-click installer: a SINGLE self-contained EXE (UAC admin manifest) with the
# IME DLL, the compiled lexicon and the Settings app embedded as resources.
set(SETUP_PAYLOAD_DLL      "${CMAKE_SOURCE_DIR}/release_package/bangla_tsf.dll")
set(SETUP_PAYLOAD_LEX      "${CMAKE_SOURCE_DIR}/release_package/data/lexicon.bin")
set(SETUP_PAYLOAD_SETTINGS "${CMAKE_SOURCE_DIR}/release_package/bangla_settings.exe")
set(SETUP_PAYLOAD_UNIVERSAL "${CMAKE_SOURCE_DIR}/release_package/likhi_universal.exe")
set(SETUP_RC               "${CMAKE_CURRENT_BINARY_DIR}/setup_rc.rc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tools/setup/setup.rc.in" "${SETUP_RC}" @ONLY)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/setup_rc.obj
    COMMAND windres -i "${SETUP_RC}" -o "${CMAKE_CURRENT_BINARY_DIR}/setup_rc.obj"
    DEPENDS "${SETUP_RC}" "${SETUP_PAYLOAD_DLL}" "${SETUP_PAYLOAD_LEX}" "${SETUP_PAYLOAD_SETTINGS}" "${SETUP_PAYLOAD_UNIVERSAL}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
    COMMENT "Compiling Installer Resources (embedded payload)"
)

add_executable(likhi_setup WIN32
    tools/setup/setup_main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/setup_rc.obj
)
target_include_directories(likhi_setup PRIVATE tsf/include engine/include)
target_link_libraries(likhi_setup PRIVATE
    ole32
    oleaut32
    user32
    advapi32
    shell32
    comctl32
    uuid      # MinGW ships the TSF CLSIDs (CLSID_TF_*) inside libuuid
)

# Engine probe (dev aid: candidate lists for Banglish words)
add_executable(probe_words tools/probe_words.cpp)
target_link_libraries(probe_words PRIVATE bangla_engine_static)

# Candidate strip visual preview (dev aid: shows the real popup with samples)
add_executable(preview_candidates
    tools/preview_candidates.cpp
    tsf/src/candidate_window.cpp
)
target_include_directories(preview_candidates PRIVATE tsf/include)
target_link_libraries(preview_candidates PRIVATE user32 gdi32)

# Performance Benchmark Target
add_executable(benchmark_perf tools/benchmark_perf.cpp)
target_link_libraries(benchmark_perf PRIVATE bangla_engine_static psapi)

# Unicode Codepoint Inspector Diagnostic Tool
add_executable(inspect_unicode tools/inspect_unicode.cpp)

# TSF COM Registration Verifier
add_executable(verify_tsf_registration tools/verify_tsf_registration.cpp)
target_link_libraries(verify_tsf_registration PRIVATE ole32 oleaut32 advapi32)

# Cloud Response Parser Regression Test Target (pure, no network)
add_executable(test_cloud_parser
    tests/unit/test_cloud_parser.cpp
    tsf/src/cloud_translit.cpp
)
target_link_libraries(test_cloud_parser PRIVATE winhttp)

# Voice Input Helper Regression Test Target (pure: no audio device, no network)
add_executable(test_voice_input
    tests/unit/test_voice_input.cpp
    tsf/src/voice_input.cpp
)
target_include_directories(test_voice_input PRIVATE tsf/include)
target_link_libraries(test_voice_input PRIVATE winmm winhttp)

# Real-World QA Runner Target
add_executable(real_world_qa_runner
    tools/real_world_qa_runner.cpp
    tsf/src/composition_mgr.cpp
    tsf/src/candidate_window.cpp
    tsf/src/edit_session.cpp
    tsf/src/cloud_translit.cpp
    tsf/src/voice_input.cpp
)
target_link_libraries(real_world_qa_runner PRIVATE
    winmm
    bangla_engine_static
    ole32
    oleaut32
    user32
    gdi32
    comctl32
    advapi32
    shell32
    psapi
    winhttp
)

# Compiler warnings, optimization, and static runtime linking flags
if (MSVC)
    target_compile_options(bangla_engine_static PRIVATE /W4 /utf-8 /O2)
    target_compile_options(bangla_tsf PRIVATE /W4 /utf-8 /O2)
    target_compile_options(cli_engine PRIVATE /W4 /utf-8)
    target_compile_options(test_runner PRIVATE /W4 /utf-8)
    target_compile_options(unicode_bengali_tests PRIVATE /W4 /utf-8)
    target_compile_options(test_tsf_integration PRIVATE /W4 /utf-8)
    target_compile_options(test_p0_keyboard PRIVATE /W4 /utf-8)
    target_compile_options(test_key_policy PRIVATE /W4 /utf-8)
    target_compile_options(bangla_settings PRIVATE /W4 /utf-8)
    target_compile_options(benchmark_perf PRIVATE /W4 /utf-8 /O2)
    target_compile_options(inspect_unicode PRIVATE /W4 /utf-8)
else()
    target_compile_options(bangla_engine_static PRIVATE -Wall -Wextra -O3)
    target_compile_options(bangla_tsf PRIVATE -Wall -Wextra -O3)
    target_compile_options(cli_engine PRIVATE -Wall -Wextra)
    target_compile_options(test_runner PRIVATE -Wall -Wextra)
    target_compile_options(unicode_bengali_tests PRIVATE -Wall -Wextra)
    target_compile_options(test_tsf_integration PRIVATE -Wall -Wextra)
    target_compile_options(test_p0_keyboard PRIVATE -Wall -Wextra)
    target_compile_options(test_key_policy PRIVATE -Wall -Wextra)
    target_compile_options(bangla_settings PRIVATE -Wall -Wextra)
    target_compile_options(benchmark_perf PRIVATE -Wall -Wextra -O3)
    target_compile_options(inspect_unicode PRIVATE -Wall -Wextra)
    target_compile_options(test_morphology PRIVATE -Wall -Wextra)
    target_compile_options(test_hybrid_brain PRIVATE -Wall -Wextra)
    target_compile_options(test_popup_quality PRIVATE -Wall -Wextra)
    target_compile_options(acceptance_verification_runner PRIVATE -Wall -Wextra)

    # Static linking for standalone DLL and EXEs without MinGW DLL runtime requirements
    target_link_options(bangla_tsf PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(bangla_settings PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(likhi_setup PRIVATE -static -static-libgcc -static-libstdc++ -municode)
    target_link_options(cli_engine PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_runner PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(unicode_bengali_tests PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_learning PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_morphology PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_hybrid_brain PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_popup_quality PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(acceptance_verification_runner PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_tsf_integration PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_p0_keyboard PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_key_policy PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_universal_typing PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_update_service PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(likhi_universal PRIVATE -static -static-libgcc -static-libstdc++ -municode)
    target_link_options(benchmark_perf PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(inspect_unicode PRIVATE -static -static-libgcc -static-libstdc++)
endif()
