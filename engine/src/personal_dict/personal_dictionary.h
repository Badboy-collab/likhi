#ifndef BANGLA_PERSONAL_DICTIONARY_H
#define BANGLA_PERSONAL_DICTIONARY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace bangla {

struct UserWordEntry {
    std::string roman_key;
    std::string bengali_word;
    uint32_t frequency;
    uint64_t last_used_timestamp;
    // false = explicitly added by the user (settings "Add word");
    // true  = auto-learned from the user committing a spelling choice.
    bool auto_learned = false;
};

class PersonalDictionary {
public:
    PersonalDictionary();
    ~PersonalDictionary() = default;

    // `auto_learned` marks the row's source: explicit user addition (settings
    // personal dictionary) vs automatic learning from commit choices. The two
    // stay logically separate so "Clear Learned Data" never wipes the user's
    // own dictionary rows (and vice versa).
    bool AddWord(const std::string& roman_key, const std::string& bengali_word,
                 bool auto_learned = false);
    bool RemoveWord(const std::string& roman_key, const std::string& bengali_word);
    bool IncrementFrequency(const std::string& bengali_word);

    std::vector<UserWordEntry> GetWordsByRomanKey(const std::string& roman_key) const;
    std::unordered_map<std::string, float> GetUserBoosts() const;
    std::vector<UserWordEntry> GetAllEntries() const;

    size_t ClearAutoLearned();     // learned-from-typing rows only
    size_t ClearExplicitUserWords(); // explicit user-added rows only
    size_t ClearAll();

    bool SaveToFile(const std::string& file_path) const;
    bool LoadFromFile(const std::string& file_path);

private:
    // Rebuilds the roman→bengali index from entries_ after a bulk clear.
    void RebuildRomanIndexLocked();
    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserWordEntry> entries_; // keyed by bengali_word
    std::unordered_map<std::string, std::vector<std::string>> roman_to_bengali_; // keyed by roman_key
};

} // namespace bangla

#endif // BANGLA_PERSONAL_DICTIONARY_H
