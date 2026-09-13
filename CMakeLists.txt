cmake_minimum_required(VERSION 3.20)
project(PC_Bangla_Typing_App VERSION 0.1.0 LANGUAGES C CXX)

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
)

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

# TSF Integration Test Target
add_executable(test_tsf_integration
    tests/tsf_integration/test_tsf_integration.cpp
    tsf/src/composition_mgr.cpp
    tsf/src/candidate_window.cpp
    tsf/src/edit_session.cpp
    tsf/src/cloud_translit.cpp
)
target_link_libraries(test_tsf_integration PRIVATE
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
    tsf/src/text_service.cpp
)
target_link_libraries(test_p0_keyboard PRIVATE
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

# Native Settings App Target
add_executable(bangla_settings WIN32
    tools/settings_app/settings_main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/settings_rc.obj
)
target_link_libraries(bangla_settings PRIVATE
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
set(SETUP_RC               "${CMAKE_CURRENT_BINARY_DIR}/setup_rc.rc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tools/setup/setup.rc.in" "${SETUP_RC}" @ONLY)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/setup_rc.obj
    COMMAND windres -i "${SETUP_RC}" -o "${CMAKE_CURRENT_BINARY_DIR}/setup_rc.obj"
    DEPENDS "${SETUP_RC}" "${SETUP_PAYLOAD_DLL}" "${SETUP_PAYLOAD_LEX}" "${SETUP_PAYLOAD_SETTINGS}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
    COMMENT "Compiling Installer Resources (embedded payload)"
)

add_executable(likhi_setup WIN32
    tools/setup/setup_main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/setup_rc.obj
)
target_include_directories(likhi_setup PRIVATE tsf/include)
target_link_libraries(likhi_setup PRIVATE
    ole32
    oleaut32
    user32
    advapi32
    shell32
    comctl32
    uuid      # MinGW ships the TSF CLSIDs (CLSID_TF_*) inside libuuid
)

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

# Real-World QA Runner Target
add_executable(real_world_qa_runner
    tools/real_world_qa_runner.cpp
    tsf/src/composition_mgr.cpp
    tsf/src/candidate_window.cpp
    tsf/src/edit_session.cpp
    tsf/src/cloud_translit.cpp
)
target_link_libraries(real_world_qa_runner PRIVATE
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

    # Static linking for standalone DLL and EXEs without MinGW DLL runtime requirements
    target_link_options(bangla_tsf PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(bangla_settings PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(likhi_setup PRIVATE -static -static-libgcc -static-libstdc++ -municode)
    target_link_options(cli_engine PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_runner PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(unicode_bengali_tests PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_learning PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_tsf_integration PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_p0_keyboard PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(test_key_policy PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(benchmark_perf PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(inspect_unicode PRIVATE -static -static-libgcc -static-libstdc++)
endif()
