#include "lexicon_trie.h"
#include "../unicode/bangla_unicode.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace bangla {

LexiconTrie::LexiconTrie()
    : string_pool_(nullptr),
      entries_(nullptr),
      bengali_indices_(nullptr),
      roman_indices_(nullptr),
      count_(0),
      str_pool_size_(0) {
}

LexiconTrie::~LexiconTrie() = default;

void LexiconTrie::Insert(const std::string& bengali_word, const std::string& roman_key, uint32_t frequency, uint16_t flags) {
    if (bengali_word.empty()) return;

    LexiconEntry entry;
    entry.bengali_word = bengali_word;
    entry.roman_key = roman_key;
    entry.frequency = frequency;
    entry.flags = flags;
    dynamic_entries_.push_back(entry);
}

const LexiconEntry* LexiconTrie::Find(const std::string& bengali_word) const {
    if (bengali_indices_ && count_ > 0 && string_pool_ && entries_) {
        int low = 0, high = static_cast<int>(count_) - 1;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            uint32_t entry_idx = bengali_indices_[mid];
            const CompactEntry& entry = entries_[entry_idx];
            std::string_view word_view(string_pool_ + entry.b_offset, entry.b_len);

            int cmp = bengali_word.compare(word_view);
            if (cmp == 0) {
                thread_local LexiconEntry tls_find_entry;
                tls_find_entry.bengali_word = std::string(word_view);
                tls_find_entry.roman_key = std::string(string_pool_ + entry.r_offset, entry.r_len);
                tls_find_entry.frequency = entry.frequency;
                tls_find_entry.flags = entry.flags;
                return &tls_find_entry;
            } else if (cmp < 0) {
                high = mid - 1;
            } else {
                low = mid + 1;
            }
        }
    }

    // Check dynamic entries fallback
    for (const auto& de : dynamic_entries_) {
        if (de.bengali_word == bengali_word) {
            return &de;
        }
    }

    return nullptr;
}

std::vector<LexiconEntry> LexiconTrie::SearchPrefix(const std::string& prefix, size_t max_results) const {
    std::vector<LexiconEntry> results;
    if (!bengali_indices_ || count_ == 0 || !string_pool_ || !entries_) return results;

    auto comp = [&](uint32_t idx, const std::string& val) {
        const CompactEntry& entry = entries_[idx];
        std::string_view b_view(string_pool_ + entry.b_offset, entry.b_len);
        return b_view < val;
    };

    auto it = std::lower_bound(bengali_indices_, bengali_indices_ + count_, prefix, comp);
    while (it != bengali_indices_ + count_) {
        const CompactEntry& entry = entries_[*it];
        std::string_view b_view(string_pool_ + entry.b_offset, entry.b_len);

        if (b_view.rfind(prefix, 0) != 0) break; // Prefix doesn't match

        LexiconEntry le;
        le.bengali_word = std::string(b_view);
        le.roman_key = std::string(string_pool_ + entry.r_offset, entry.r_len);
        le.frequency = entry.frequency;
        le.flags = entry.flags;
        results.push_back(le);

        if (results.size() >= max_results * 2) break;
        ++it;
    }

    std::sort(results.begin(), results.end(), [](const LexiconEntry& a, const LexiconEntry& b) {
        return a.frequency > b.frequency;
    });

    if (results.size() > max_results) {
        results.resize(max_results);
    }
    return results;
}

std::vector<LexiconEntry> LexiconTrie::SearchRoman(const std::string& roman_prefix, size_t max_results) const {
    std::vector<LexiconEntry> results;

    if (roman_indices_ && count_ > 0 && string_pool_ && entries_) {
        auto comp = [&](uint32_t idx, const std::string& val) {
            const CompactEntry& entry = entries_[idx];
            std::string_view r_view(string_pool_ + entry.r_offset, entry.r_len);
            return r_view < val;
        };

        auto it = std::lower_bound(roman_indices_, roman_indices_ + count_, roman_prefix, comp);
        while (it != roman_indices_ + count_) {
            const CompactEntry& entry = entries_[*it];
            std::string_view r_view(string_pool_ + entry.r_offset, entry.r_len);

            if (r_view != roman_prefix) break; // End of exact matching range

            LexiconEntry le;
            le.bengali_word = std::string(string_pool_ + entry.b_offset, entry.b_len);
            le.roman_key = std::string(r_view);
            le.frequency = entry.frequency;
            le.flags = entry.flags;
            results.push_back(le);

            if (results.size() >= max_results) break;
            ++it;
        }
    }

    // Check dynamic entries
    for (const auto& de : dynamic_entries_) {
        if (de.roman_key == roman_prefix) {
            results.push_back(de);
            if (results.size() >= max_results) break;
        }
    }

    std::sort(results.begin(), results.end(), [](const LexiconEntry& a, const LexiconEntry& b) {
        return a.frequency > b.frequency;
    });

    if (results.size() > max_results) {
        results.resize(max_results);
    }
    return results;
}

bool LexiconTrie::SaveToFile(const std::string& binary_path) const {
    // Serialization is handled by build_lexicon.py tool
    return true;
}

bool LexiconTrie::LoadFromFile(const std::string& binary_path) {
    std::ifstream in(binary_path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;

    std::streamsize file_size = in.tellg();
    in.seekg(0, std::ios::beg);

    if (file_size < 16) return false;

    buffer_.resize(static_cast<size_t>(file_size));
    if (!in.read(reinterpret_cast<char*>(buffer_.data()), file_size)) {
        buffer_.clear();
        return false;
    }

    uint32_t magic = *reinterpret_cast<const uint32_t*>(buffer_.data());
    uint32_t version = *reinterpret_cast<const uint32_t*>(buffer_.data() + 4);
    count_ = *reinterpret_cast<const uint32_t*>(buffer_.data() + 8);
    str_pool_size_ = *reinterpret_cast<const uint32_t*>(buffer_.data() + 12);

    if (magic != 0x4C474E42 || version != 2) {
        buffer_.clear();
        count_ = 0;
        return false;
    }

    size_t offset = 16;
    string_pool_ = reinterpret_cast<const char*>(buffer_.data() + offset);
    offset += str_pool_size_;

    entries_ = reinterpret_cast<const CompactEntry*>(buffer_.data() + offset);
    offset += count_ * sizeof(CompactEntry);

    bengali_indices_ = reinterpret_cast<const uint32_t*>(buffer_.data() + offset);
    offset += count_ * sizeof(uint32_t);

    roman_indices_ = reinterpret_cast<const uint32_t*>(buffer_.data() + offset);
    offset += count_ * sizeof(uint32_t);

    return true;
}

void LexiconTrie::LoadDefaultVocabulary() {
    if (LoadFromFile("engine/data/lexicon.bin") ||
        LoadFromFile("../engine/data/lexicon.bin") ||
        LoadFromFile("../../engine/data/lexicon.bin")) {
        return;
    }
}

} // namespace bangla
