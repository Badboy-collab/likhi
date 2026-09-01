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
};

class PersonalDictionary {
public:
    PersonalDictionary();
    ~PersonalDictionary() = default;

    bool AddWord(const std::string& roman_key, const std::string& bengali_word);
    bool RemoveWord(const std::string& roman_key, const std::string& bengali_word);
    bool IncrementFrequency(const std::string& bengali_word);

    std::vector<UserWordEntry> GetWordsByRomanKey(const std::string& roman_key) const;
    std::unordered_map<std::string, float> GetUserBoosts() const;
    std::vector<UserWordEntry> GetAllEntries() const;

    bool SaveToFile(const std::string& file_path) const;
    bool LoadFromFile(const std::string& file_path);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserWordEntry> entries_; // keyed by bengali_word
    std::unordered_map<std::string, std::vector<std::string>> roman_to_bengali_; // keyed by roman_key
};

} // namespace bangla

#endif // BANGLA_PERSONAL_DICTIONARY_H
