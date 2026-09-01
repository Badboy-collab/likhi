#include "personal_dictionary.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace bangla {

PersonalDictionary::PersonalDictionary() = default;

bool PersonalDictionary::AddWord(const std::string& roman_key, const std::string& bengali_word) {
    if (roman_key.empty() || bengali_word.empty()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    auto it = entries_.find(bengali_word);
    if (it != entries_.end()) {
        it->second.frequency++;
        it->second.last_used_timestamp = now;
    } else {
        UserWordEntry entry;
        entry.roman_key = roman_key;
        entry.bengali_word = bengali_word;
        entry.frequency = 1;
        entry.last_used_timestamp = now;
        entries_[bengali_word] = entry;

        roman_to_bengali_[roman_key].push_back(bengali_word);
    }
    return true;
}

bool PersonalDictionary::RemoveWord(const std::string& roman_key, const std::string& bengali_word) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(bengali_word);
    if (it != entries_.end()) {
        entries_.erase(it);

        auto rit = roman_to_bengali_.find(roman_key);
        if (rit != roman_to_bengali_.end()) {
            auto& vec = rit->second;
            vec.erase(std::remove(vec.begin(), vec.end(), bengali_word), vec.end());
            if (vec.empty()) {
                roman_to_bengali_.erase(rit);
            }
        }
        return true;
    }
    return false;
}

bool PersonalDictionary::IncrementFrequency(const std::string& bengali_word) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(bengali_word);
    if (it != entries_.end()) {
        it->second.frequency++;
        it->second.last_used_timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return true;
    }
    return false;
}

std::vector<UserWordEntry> PersonalDictionary::GetWordsByRomanKey(const std::string& roman_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserWordEntry> list;
    auto rit = roman_to_bengali_.find(roman_key);
    if (rit != roman_to_bengali_.end()) {
        for (const auto& bword : rit->second) {
            auto eit = entries_.find(bword);
            if (eit != entries_.end()) {
                list.push_back(eit->second);
            }
        }
    }
    return list;
}

std::unordered_map<std::string, float> PersonalDictionary::GetUserBoosts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, float> boosts;
    for (const auto& pair : entries_) {
        float b = std::min(1.0f, 0.2f + std::log10(static_cast<float>(pair.second.frequency) + 1.0f) / 3.0f);
        boosts[pair.first] = b;
    }
    return boosts;
}

std::vector<UserWordEntry> PersonalDictionary::GetAllEntries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserWordEntry> list;
    list.reserve(entries_.size());
    for (const auto& pair : entries_) {
        list.push_back(pair.second);
    }
    return list;
}

bool PersonalDictionary::SaveToFile(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(file_path);
    if (!out.is_open()) return false;

    for (const auto& pair : entries_) {
        out << pair.second.roman_key << "\t"
            << pair.second.bengali_word << "\t"
            << pair.second.frequency << "\t"
            << pair.second.last_used_timestamp << "\n";
    }
    return true;
}

bool PersonalDictionary::LoadFromFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream in(file_path);
    if (!in.is_open()) return false;

    entries_.clear();
    roman_to_bengali_.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string roman, bengali;
        uint32_t freq = 1;
        uint64_t timestamp = 0;

        if (std::getline(ss, roman, '\t') && std::getline(ss, bengali, '\t')) {
            ss >> freq >> timestamp;
            UserWordEntry entry;
            entry.roman_key = roman;
            entry.bengali_word = bengali;
            entry.frequency = freq;
            entry.last_used_timestamp = timestamp;
            entries_[bengali] = entry;
            roman_to_bengali_[roman].push_back(bengali);
        }
    }
    return true;
}

} // namespace bangla
