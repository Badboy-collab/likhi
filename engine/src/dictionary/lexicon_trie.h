#ifndef BANGLA_LEXICON_TRIE_H
#define BANGLA_LEXICON_TRIE_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <string_view>

namespace bangla {

struct LexiconEntry {
    std::string bengali_word;
    std::string roman_key;
    uint32_t frequency;
    uint16_t flags;
};

#pragma pack(push, 1)
struct CompactEntry {
    uint32_t b_offset;
    uint16_t b_len;
    uint32_t r_offset;
    uint16_t r_len;
    uint32_t frequency;
    uint16_t flags;
    uint16_t pad;
};
#pragma pack(pop)

class LexiconTrie {
public:
    LexiconTrie();
    ~LexiconTrie();

    void Insert(const std::string& bengali_word, const std::string& roman_key, uint32_t frequency, uint16_t flags = 0);
    const LexiconEntry* Find(const std::string& bengali_word) const;
    std::vector<LexiconEntry> SearchPrefix(const std::string& prefix, size_t max_results = 10) const;
    std::vector<LexiconEntry> SearchRoman(const std::string& roman_prefix, size_t max_results = 10) const;

    bool LoadFromFile(const std::string& binary_path);
    bool SaveToFile(const std::string& binary_path) const;
    void LoadDefaultVocabulary();

    size_t WordCount() const { return count_; }
    size_t MemoryUsageBytes() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
    const char* string_pool_;
    const CompactEntry* entries_;
    const uint32_t* bengali_indices_;
    const uint32_t* roman_indices_;
    uint32_t count_;
    uint32_t str_pool_size_;

    // Fallback dynamic entries for runtime insertions (if any)
    std::vector<LexiconEntry> dynamic_entries_;
};

} // namespace bangla

#endif // BANGLA_LEXICON_TRIE_H
