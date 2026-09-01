#include "../engine/include/bangla_engine.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

void SetupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void PrintHelp() {
    std::cout << "\n=== PC Bangla Typing App - Standalone CLI Engine ===\n";
    std::cout << "Commands:\n";
    std::cout << "  :help             - Show this help message\n";
    std::cout << "  :autocorrect on   - Enable Auto-Correct mode\n";
    std::cout << "  :autocorrect off  - Disable Auto-Correct mode\n";
    std::cout << "  :add <key> <word> - Add custom word to Personal Dictionary\n";
    std::cout << "  :quit / :q        - Exit\n";
    std::cout << "Type any Romanized Bengali (Banglish) sentence or word to test candidates:\n\n";
}

int main(int argc, char* argv[]) {
    SetupConsole();

    EngineConfig config;
    BanglaEngine_GetDefaultConfig(&config);
    config.auto_correct_enabled = false;
    config.lexicon_binary_path = "engine/data/lexicon.bin";

    BanglaEngine* engine = BanglaEngine_Create(&config);

    // Non-interactive single sentence execution
    if (argc >= 3 && strcmp(argv[1], "--sentence") == 0) {
        char sentence_out[2048] = {0};
        BanglaEngine_TransliterateSentence(engine, argv[2], sentence_out, sizeof(sentence_out));
        std::cout << sentence_out << "\n";
        BanglaEngine_Destroy(engine);
        return 0;
    }

    // High-speed batch processing via stdin
    if (argc >= 2 && strcmp(argv[1], "--batch") == 0) {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                std::cout << "\n";
                continue;
            }
            char sentence_out[2048] = {0};
            BanglaEngine_TransliterateSentence(engine, line.c_str(), sentence_out, sizeof(sentence_out));
            std::cout << sentence_out << "\n";
        }
        BanglaEngine_Destroy(engine);
        return 0;
    }

    std::cout << "=========================================================\n";
    std::cout << "  PC Bangla Typing App - Standalone Engine Prototype v0.1\n";
    std::cout << "  Auto-Correct: " << (config.auto_correct_enabled ? "ON" : "OFF") << "\n";
    std::cout << "=========================================================\n";
    PrintHelp();

    std::string line;
    while (true) {
        std::cout << "Banglish> ";
        if (!std::getline(std::cin, line)) break;

        if (line.empty()) continue;

        if (line == ":q" || line == ":quit" || line == "exit") {
            break;
        } else if (line == ":help") {
            PrintHelp();
            continue;
        } else if (line == ":autocorrect on") {
            config.auto_correct_enabled = true;
            BanglaEngine_Destroy(engine);
            engine = BanglaEngine_Create(&config);
            std::cout << "[Config] Auto-Correct is now ON (Confidence Threshold: " << config.auto_correct_threshold << ")\n\n";
            continue;
        } else if (line == ":autocorrect off") {
            config.auto_correct_enabled = false;
            BanglaEngine_Destroy(engine);
            engine = BanglaEngine_Create(&config);
            std::cout << "[Config] Auto-Correct is now OFF (Strict Candidate Suggestions Only)\n\n";
            continue;
        } else if (line.rfind(":add ", 0) == 0) {
            std::string rem = line.substr(5);
            size_t space_pos = rem.find(' ');
            if (space_pos != std::string::npos) {
                std::string key = rem.substr(0, space_pos);
                std::string word = rem.substr(space_pos + 1);
                BanglaEngine_AddUserWord(engine, key.c_str(), word.c_str());
                std::cout << "[Personal Dict] Added: " << key << " -> " << word << "\n\n";
            } else {
                std::cout << "[Error] Usage: :add <roman_key> <bengali_word>\n\n";
            }
            continue;
        }

        // Full sentence transliteration preview
        char sentence_out[1024] = {0};
        BanglaEngine_TransliterateSentence(engine, line.c_str(), sentence_out, sizeof(sentence_out));
        std::cout << "Bengali Sentence: " << sentence_out << "\n";

        // Word-level candidate analysis
        BanglaEngine_ResetComposition(engine);
        BanglaEngine_SetComposition(engine, line.c_str());

        CandidateList list;
        BanglaEngine_GetCandidates(engine, &list);

        std::cout << "Word Candidates (" << list.count << "):\n";
        for (uint32_t i = 0; i < list.count; i++) {
            std::cout << "  [" << (i + 1) << "] " << list.candidates[i].bengali_text
                      << "  (Score: " << list.candidates[i].score
                      << (list.candidates[i].auto_correct_recommended ? " | AUTO-CORRECT RECOMMENDED" : "")
                      << ")\n";
        }
        std::cout << "\n";
    }

    BanglaEngine_Destroy(engine);
    return 0;
}
