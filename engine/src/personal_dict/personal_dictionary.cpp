#include "personal_dictionary.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace bangla {

PersonalDictionary::PersonalDictionary() = default;

std::string PersonalDictionary::ToLower(const std::string& str) {
    std::string lower = str;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

std::string PersonalDictionary::MakeKey(const std::string& roman, const std::string& bengali) {
    return ToLower(roman) + "\t" + bengali;
}

void PersonalDictionary::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    roman_to_bengali_.clear();
    next_id_ = 1;
}

bool PersonalDictionary::HasWord(const std::string& roman_key, const std::string& bengali_word) const {
    if (roman_key.empty() || bengali_word.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.find(MakeKey(roman_key, bengali_word)) != entries_.end();
}

bool PersonalDictionary::HasRomanKey(const std::string& roman_key) const {
    if (roman_key.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = roman_to_bengali_.find(ToLower(roman_key));
    return it != roman_to_bengali_.end() && !it->second.empty();
}

bool PersonalDictionary::AddWord(const std::string& roman_key, const std::string& bengali_word) {
    if (roman_key.empty() || bengali_word.empty()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    std::string key = MakeKey(roman_key, bengali_word);
    std::string lower_roman = ToLower(roman_key);

    auto it = entries_.find(key);
    if (it != entries_.end()) {
        it->second.frequency++;
        it->second.updated_at = now;
        it->second.last_used_timestamp = now;
        auto& list = roman_to_bengali_[lower_roman];
        if (std::find(list.begin(), list.end(), bengali_word) == list.end()) {
            list.push_back(bengali_word);
        }
    } else {
        UserWordEntry entry;
        entry.id = std::to_string(next_id_++);
        entry.roman_key = lower_roman;
        entry.bengali_word = bengali_word;
        entry.frequency = 1;
        entry.created_at = now;
        entry.updated_at = now;
        entry.last_used_timestamp = now;
        entries_[key] = entry;

        auto& list = roman_to_bengali_[lower_roman];
        if (std::find(list.begin(), list.end(), bengali_word) == list.end()) {
            list.push_back(bengali_word);
        }
    }
    return true;
}

bool PersonalDictionary::UpdateWord(const std::string& old_roman, const std::string& old_bengali,
                                   const std::string& new_roman, const std::string& new_bengali) {
    if (old_roman.empty() || old_bengali.empty() || new_roman.empty() || new_bengali.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string old_key = MakeKey(old_roman, old_bengali);
    auto it = entries_.find(old_key);
    if (it == entries_.end()) {
        return false;
    }

    UserWordEntry existing = it->second;
    entries_.erase(it);

    std::string old_lower = ToLower(old_roman);
    auto rit = roman_to_bengali_.find(old_lower);
    if (rit != roman_to_bengali_.end()) {
        auto& vec = rit->second;
        vec.erase(std::remove(vec.begin(), vec.end(), old_bengali), vec.end());
        if (vec.empty()) {
            roman_to_bengali_.erase(rit);
        }
    }

    auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    existing.roman_key = ToLower(new_roman);
    existing.bengali_word = new_bengali;
    existing.updated_at = now;
    existing.last_used_timestamp = now;

    std::string new_key = MakeKey(new_roman, new_bengali);
    std::string new_lower = ToLower(new_roman);
    entries_[new_key] = existing;

    auto& list = roman_to_bengali_[new_lower];
    if (std::find(list.begin(), list.end(), new_bengali) == list.end()) {
        list.push_back(new_bengali);
    }

    return true;
}

bool PersonalDictionary::RemoveWord(const std::string& roman_key, const std::string& bengali_word) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = MakeKey(roman_key, bengali_word);
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        entries_.erase(it);

        std::string lower_roman = ToLower(roman_key);
        auto rit = roman_to_bengali_.find(lower_roman);
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
    auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    bool found = false;
    for (auto& pair : entries_) {
        if (pair.second.bengali_word == bengali_word) {
            pair.second.frequency++;
            pair.second.updated_at = now;
            pair.second.last_used_timestamp = now;
            found = true;
        }
    }
    return found;
}

std::vector<UserWordEntry> PersonalDictionary::GetWordsByRomanKey(const std::string& roman_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserWordEntry> list;
    std::string lower_roman = ToLower(roman_key);
    auto rit = roman_to_bengali_.find(lower_roman);
    if (rit != roman_to_bengali_.end()) {
        for (const auto& bword : rit->second) {
            auto eit = entries_.find(MakeKey(lower_roman, bword));
            if (eit != entries_.end()) {
                list.push_back(eit->second);
            }
        }
    }
    std::sort(list.begin(), list.end(), [](const UserWordEntry& a, const UserWordEntry& b) {
        if (a.frequency != b.frequency) return a.frequency > b.frequency;
        return a.last_used_timestamp > b.last_used_timestamp;
    });
    return list;
}

std::unordered_map<std::string, float> PersonalDictionary::GetUserBoosts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, float> boosts;
    for (const auto& pair : entries_) {
        float b = std::min(1.0f, 0.2f + std::log10(static_cast<float>(pair.second.frequency) + 1.0f) / 3.0f);
        boosts[pair.second.bengali_word] = b;
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
    if (file_path.empty()) return false;

    std::string temp_path = file_path + ".tmp";
    {
        std::ofstream out(temp_path, std::ios::trunc);
        if (!out.is_open()) return false;

        for (const auto& pair : entries_) {
            out << pair.second.roman_key << "\t"
                << pair.second.bengali_word << "\t"
                << pair.second.frequency << "\t"
                << pair.second.created_at << "\t"
                << pair.second.updated_at << "\t"
                << pair.second.id << "\n";
        }
        out.flush();
    }

#ifdef _WIN32
    std::wstring wtemp(temp_path.begin(), temp_path.end());
    std::wstring wtarget(file_path.begin(), file_path.end());
    if (!MoveFileExW(wtemp.c_str(), wtarget.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::remove(file_path.c_str());
        if (std::rename(temp_path.c_str(), file_path.c_str()) != 0) {
            return false;
        }
    }
#else
    if (std::rename(temp_path.c_str(), file_path.c_str()) != 0) {
        std::remove(file_path.c_str());
        return std::rename(temp_path.c_str(), file_path.c_str()) == 0;
    }
#endif
    return true;
}

bool PersonalDictionary::LoadFromFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream in(file_path);
    if (!in.is_open()) return false;

    entries_.clear();
    roman_to_bengali_.clear();
    next_id_ = 1;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string roman, bengali, token;
        std::vector<std::string> tokens;

        while (std::getline(ss, token, '\t')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 2) continue;

        roman = ToLower(tokens[0]);
        bengali = tokens[1];
        if (roman.empty() || bengali.empty()) continue;

        uint32_t freq = 1;
        uint64_t created = 0;
        uint64_t updated = 0;
        std::string id_str = "";

        if (tokens.size() >= 3) {
            try {
                freq = static_cast<uint32_t>(std::stoul(tokens[2]));
                if (freq == 0) freq = 1;
            } catch (...) {
                freq = 1;
            }
        }
        if (tokens.size() >= 4) {
            try {
                created = static_cast<uint64_t>(std::stoull(tokens[3]));
            } catch (...) {
                created = 0;
            }
        }
        if (tokens.size() >= 5) {
            try {
                updated = static_cast<uint64_t>(std::stoull(tokens[4]));
            } catch (...) {
                updated = created;
            }
        } else {
            updated = created;
        }
        if (tokens.size() >= 6) {
            id_str = tokens[5];
        } else {
            id_str = std::to_string(next_id_++);
        }

        std::string key = MakeKey(roman, bengali);
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            // Merge duplicate lines safely
            it->second.frequency = std::max(it->second.frequency, freq);
            it->second.updated_at = std::max(it->second.updated_at, updated);
            it->second.last_used_timestamp = it->second.updated_at;
        } else {
            UserWordEntry entry;
            entry.id = id_str;
            entry.roman_key = roman;
            entry.bengali_word = bengali;
            entry.frequency = freq;
            entry.created_at = created;
            entry.updated_at = updated;
            entry.last_used_timestamp = updated;
            entries_[key] = entry;

            auto& vec = roman_to_bengali_[roman];
            if (std::find(vec.begin(), vec.end(), bengali) == vec.end()) {
                vec.push_back(bengali);
            }
        }
    }
    return true;
}

} // namespace bangla
