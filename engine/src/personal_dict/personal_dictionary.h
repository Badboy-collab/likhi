#ifndef BANGLA_PERSONAL_DICTIONARY_H
#define BANGLA_PERSONAL_DICTIONARY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace bangla {

struct UserWordEntry {
    std::string id;
    std::string roman_key;
    std::string bengali_word;
    uint32_t frequency = 1;
    uint64_t created_at = 0;
    uint64_t updated_at = 0;
    uint64_t last_used_timestamp = 0; // alias for backwards-compatibility
};

class PersonalDictionary {
public:
    PersonalDictionary();
    ~PersonalDictionary() = default;

    bool AddWord(const std::string& roman_key, const std::string& bengali_word);
    bool UpdateWord(const std::string& old_roman, const std::string& old_bengali,
                    const std::string& new_roman, const std::string& new_bengali);
    bool RemoveWord(const std::string& roman_key, const std::string& bengali_word);
    bool HasWord(const std::string& roman_key, const std::string& bengali_word) const;
    bool HasRomanKey(const std::string& roman_key) const;
    bool IncrementFrequency(const std::string& bengali_word);

    std::vector<UserWordEntry> GetWordsByRomanKey(const std::string& roman_key) const;
    std::unordered_map<std::string, float> GetUserBoosts() const;
    std::vector<UserWordEntry> GetAllEntries() const;

    bool SaveToFile(const std::string& file_path) const;
    bool LoadFromFile(const std::string& file_path);
    void Clear();

private:
    static std::string ToLower(const std::string& str);
    static std::string MakeKey(const std::string& roman, const std::string& bengali);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserWordEntry> entries_; // keyed by ToLower(roman) + "\t" + bengali
    std::unordered_map<std::string, std::vector<std::string>> roman_to_bengali_; // keyed by ToLower(roman)
    uint64_t next_id_ = 1;
};

} // namespace bangla

#endif // BANGLA_PERSONAL_DICTIONARY_H

