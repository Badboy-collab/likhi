// engine/src/global_model/global_model_reader.cpp
// Phase 3.1 — Global Model Reader Implementation
//
// All file I/O is read-only. No network. No writes.
// NEVER modifies any file or user data.

#include "global_model_reader.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace bangla {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

GlobalModelReader::GlobalModelReader()
    : loaded_(false),
      last_result_(ModelLoadResult::kFileNotFound)
#ifdef _WIN32
      , hFile_(INVALID_HANDLE_VALUE)
      , hMapping_(nullptr)
      , mapped_view_(nullptr)
#else
      , fd_(-1)
      , mapped_view_(nullptr)
#endif
      , file_size_(0)
      , header_(nullptr)
      , bucket_table_(nullptr)
      , entry_table_(nullptr)
      , string_pool_(nullptr)
      , entry_count_(0)
      , bucket_count_(0)
      , sp_size_(0)
{
}

GlobalModelReader::~GlobalModelReader() {
    Close();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool GlobalModelReader::IsLoaded() const {
    return loaded_;
}

uint32_t GlobalModelReader::GetModelVersion() const {
    if (!loaded_ || !header_) return 0;
    return header_->global_model_version;
}

uint32_t GlobalModelReader::GetEntryCount() const {
    return loaded_ ? entry_count_ : 0;
}

ModelLoadResult GlobalModelReader::GetLastLoadResult() const {
    return last_result_;
}

void GlobalModelReader::Close() {
    loaded_       = false;
    header_       = nullptr;
    bucket_table_ = nullptr;
    entry_table_  = nullptr;
    string_pool_  = nullptr;
    entry_count_  = 0;
    bucket_count_ = 0;
    sp_size_      = 0;
    file_size_    = 0;
    CloseHandles();
}

// ---------------------------------------------------------------------------
// Open — map the file and run all validation checks
// ---------------------------------------------------------------------------

ModelLoadResult GlobalModelReader::Open(const std::string& path) {
    // Always clean up any previous state first
    Close();
    last_result_ = ModelLoadResult::kFileNotFound;

    if (path.empty()) {
        return last_result_;
    }

#ifdef _WIN32
    // --- Windows: CreateFileW (read-only) + CreateFileMapping + MapViewOfFile ---
    std::wstring wpath(path.begin(), path.end());
    hFile_ = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
        nullptr
    );
    if (hFile_ == INVALID_HANDLE_VALUE) {
        last_result_ = ModelLoadResult::kFileNotFound;
        return last_result_;
    }

    LARGE_INTEGER li;
    if (!GetFileSizeEx(hFile_, &li) || li.QuadPart == 0) {
        CloseHandles();
        last_result_ = ModelLoadResult::kFileEmpty;
        return last_result_;
    }
    file_size_ = static_cast<size_t>(li.QuadPart);

    if (file_size_ < sizeof(Lgm1Header)) {
        CloseHandles();
        last_result_ = ModelLoadResult::kFileTooSmall;
        return last_result_;
    }

    hMapping_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping_) {
        CloseHandles();
        last_result_ = ModelLoadResult::kMapFailed;
        return last_result_;
    }

    mapped_view_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0);
    if (!mapped_view_) {
        CloseHandles();
        last_result_ = ModelLoadResult::kMapFailed;
        return last_result_;
    }

#else
    // --- POSIX: open + mmap (for unit testing on non-Windows) ---
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        last_result_ = ModelLoadResult::kFileNotFound;
        return last_result_;
    }

    struct stat st;
    if (::fstat(fd_, &st) != 0 || st.st_size == 0) {
        CloseHandles();
        last_result_ = ModelLoadResult::kFileEmpty;
        return last_result_;
    }
    file_size_ = static_cast<size_t>(st.st_size);

    if (file_size_ < sizeof(Lgm1Header)) {
        CloseHandles();
        last_result_ = ModelLoadResult::kFileTooSmall;
        return last_result_;
    }

    mapped_view_ = ::mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mapped_view_ == MAP_FAILED) {
        mapped_view_ = nullptr;
        CloseHandles();
        last_result_ = ModelLoadResult::kMapFailed;
        return last_result_;
    }
#endif

    // -------------------------------------------------------------------
    // Step 1: Cast and validate header
    // -------------------------------------------------------------------
    const auto* h = static_cast<const Lgm1Header*>(mapped_view_);

    if (!ValidateHeader(h, file_size_)) {
        Close();
        // last_result_ already set inside ValidateHeader
        return last_result_;
    }

    // -------------------------------------------------------------------
    // Step 2: Cache validated field values and set pointer views
    // -------------------------------------------------------------------
    entry_count_  = h->entry_count;
    bucket_count_ = h->bucket_count;
    sp_size_      = h->string_pool_size;

    const auto* base = static_cast<const uint8_t*>(mapped_view_);

    bucket_table_ = reinterpret_cast<const uint32_t*>(base + h->bucket_table_offset);
    entry_table_  = reinterpret_cast<const ModelEntry*>(base + h->entry_table_offset);
    string_pool_  = reinterpret_cast<const char*>(base + h->string_pool_offset);
    header_       = h;

    // -------------------------------------------------------------------
    // Step 3: Validate all individual entries (bounds + content)
    // -------------------------------------------------------------------
    if (!ValidateAllEntries()) {
        Close();
        // last_result_ set inside ValidateAllEntries
        return last_result_;
    }

    loaded_      = true;
    last_result_ = ModelLoadResult::kOK;
    return last_result_;
}

// ---------------------------------------------------------------------------
// GetCandidateScore — O(1) hash-bucket lookup, no allocations, no blocking
// ---------------------------------------------------------------------------

float GlobalModelReader::GetCandidateScore(const std::string& roman_key,
                                            const std::string& bengali_word) const {
    if (!loaded_) return 0.0f;
    if (roman_key.empty() || bengali_word.empty()) return 0.0f;
    if (roman_key.size() > kMaxRomanKeyBytes) return 0.0f;

    const uint32_t bucket_idx = HashRomanKey(roman_key.c_str(), roman_key.size()) % bucket_count_;
    uint32_t entry_idx = bucket_table_[bucket_idx];

    while (entry_idx != 0xFFFFFFFF) {
        if (entry_idx >= entry_count_) break;  // corrupt chain guard

        const ModelEntry& e = entry_table_[entry_idx];

        // Check roman_key match
        if (e.roman_len == static_cast<uint16_t>(roman_key.size()) &&
            std::memcmp(string_pool_ + e.roman_offset, roman_key.c_str(), roman_key.size()) == 0) {

            // Check bengali_word match
            if (e.bengali_len == static_cast<uint16_t>(bengali_word.size()) &&
                std::memcmp(string_pool_ + e.bengali_offset, bengali_word.c_str(), bengali_word.size()) == 0) {

                return static_cast<float>(e.frequency_score) / 10000.0f;
            }
        }

        entry_idx = e.next_entry;
    }

    return 0.0f;
}

// ---------------------------------------------------------------------------
// ValidateHeader — checks magic, schema, bounds, reserved fields
// ---------------------------------------------------------------------------

bool GlobalModelReader::ValidateHeader(const Lgm1Header* h, size_t file_size) const {
    // Magic
    if (h->magic[0] != 'L' || h->magic[1] != 'G' ||
        h->magic[2] != 'M' || h->magic[3] != '1') {
        last_result_ = ModelLoadResult::kMagicMismatch;
        return false;
    }

    // Schema version — only version 1 is supported
    if (h->schema_version != 1) {
        last_result_ = ModelLoadResult::kSchemaUnsupported;
        return false;
    }

    // Reserved fields must be zero (future use, corruption detection)
    if (h->reserved != 0 || h->reserved2 != 0) {
        last_result_ = ModelLoadResult::kReservedNonZero;
        return false;
    }

    // Entry count safety cap
    if (h->entry_count > kMaxModelEntries) {
        last_result_ = ModelLoadResult::kEntryCountExceedsMax;
        return false;
    }

    // Bucket count safety cap (must be > 0 if entries exist)
    if (h->bucket_count == 0 || h->bucket_count > kMaxBucketCount) {
        last_result_ = ModelLoadResult::kBucketCountExceedsMax;
        return false;
    }

    // String pool size cap
    if (h->string_pool_size > kMaxStringPoolBytes) {
        last_result_ = ModelLoadResult::kStringPoolTooBig;
        return false;
    }

    // String pool bounds: [string_pool_offset, string_pool_offset + string_pool_size)
    // must fit entirely within the file.
    {
        uint64_t sp_end = static_cast<uint64_t>(h->string_pool_offset) +
                          static_cast<uint64_t>(h->string_pool_size);
        if (h->string_pool_offset < sizeof(Lgm1Header) || sp_end > file_size) {
            last_result_ = ModelLoadResult::kOffsetOutOfBounds;
            return false;
        }
    }

    // Bucket table bounds
    {
        uint64_t bt_size = static_cast<uint64_t>(h->bucket_count) * sizeof(uint32_t);
        uint64_t bt_end  = static_cast<uint64_t>(h->bucket_table_offset) + bt_size;
        if (h->bucket_table_offset < sizeof(Lgm1Header) || bt_end > file_size) {
            last_result_ = ModelLoadResult::kBucketTableOutOfBounds;
            return false;
        }
    }

    // Entry table bounds
    {
        uint64_t et_size = static_cast<uint64_t>(h->entry_count) * sizeof(ModelEntry);
        uint64_t et_end  = static_cast<uint64_t>(h->entry_table_offset) + et_size;
        if (h->entry_table_offset < sizeof(Lgm1Header) || et_end > file_size) {
            last_result_ = ModelLoadResult::kEntryTableOutOfBounds;
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// ValidateAllEntries — per-entry field bounds + content checks
// ---------------------------------------------------------------------------

bool GlobalModelReader::ValidateAllEntries() const {
    const auto* base = static_cast<const uint8_t*>(mapped_view_);

    // We impose a chain-length limit to guard against circular references
    // in the linked-list bucket chains.
    constexpr uint32_t kMaxChainLength = 256;

    for (uint32_t b = 0; b < bucket_count_; ++b) {
        uint32_t entry_idx = bucket_table_[b];
        uint32_t chain_len = 0;

        while (entry_idx != 0xFFFFFFFF) {
            // Guard: entry_idx must be within entry_count_
            if (entry_idx >= entry_count_) {
                last_result_ = ModelLoadResult::kInvalidEntry;
                return false;
            }

            // Guard: chain length must not exceed limit (detects circular refs)
            if (++chain_len > kMaxChainLength) {
                last_result_ = ModelLoadResult::kInvalidEntry;
                return false;
            }

            const ModelEntry& e = entry_table_[entry_idx];

            // roman_len must be 1..kMaxRomanKeyBytes
            if (e.roman_len == 0 || e.roman_len > kMaxRomanKeyBytes) {
                last_result_ = ModelLoadResult::kInvalidEntry;
                return false;
            }

            // bengali_len must be 1..kMaxBengaliBytes
            if (e.bengali_len == 0 || e.bengali_len > kMaxBengaliBytes) {
                last_result_ = ModelLoadResult::kInvalidEntry;
                return false;
            }

            // roman string bounds within string pool
            {
                uint64_t r_end = static_cast<uint64_t>(e.roman_offset) + e.roman_len;
                if (r_end > sp_size_) {
                    last_result_ = ModelLoadResult::kInvalidEntry;
                    return false;
                }
            }

            // bengali string bounds within string pool
            {
                uint64_t b_end = static_cast<uint64_t>(e.bengali_offset) + e.bengali_len;
                if (b_end > sp_size_) {
                    last_result_ = ModelLoadResult::kInvalidEntry;
                    return false;
                }
            }

            // roman key: must be all lowercase ASCII letters [a-z]
            if (!IsValidRomanKey(string_pool_ + e.roman_offset, e.roman_len)) {
                last_result_ = ModelLoadResult::kInvalidEntry;
                return false;
            }

            // bengali word: must be valid UTF-8 containing only Bangla Unicode block
            if (!IsValidBengaliUtf8(string_pool_ + e.bengali_offset, e.bengali_len)) {
                last_result_ = ModelLoadResult::kInvalidEntry;
                return false;
            }

            entry_idx = e.next_entry;
        }
    }

    (void)base; // suppress unused warning
    return true;
}

// ---------------------------------------------------------------------------
// IsValidRomanKey — [a-z]{1,24} only
// ---------------------------------------------------------------------------

bool GlobalModelReader::IsValidRomanKey(const char* bytes, uint16_t len) const {
    if (len == 0 || len > kMaxRomanKeyBytes) return false;
    for (uint16_t i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c < 'a' || c > 'z') return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// IsValidBengaliUtf8 — must be valid UTF-8 and all codepoints in Bangla block
//                      U+0980..U+09FF plus common joiners/combining marks.
// ---------------------------------------------------------------------------

bool GlobalModelReader::IsValidBengaliUtf8(const char* bytes, uint16_t len) const {
    if (len == 0 || len > kMaxBengaliBytes) return false;

    uint16_t i = 0;
    while (i < len) {
        unsigned char b0 = static_cast<unsigned char>(bytes[i]);
        uint32_t codepoint = 0;
        uint16_t seq_len = 0;

        if (b0 < 0x80) {
            // ASCII: not valid in a Bengali-only word
            return false;
        } else if ((b0 & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= len) return false;
            unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
            if ((b1 & 0xC0) != 0x80) return false;
            codepoint = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
            seq_len = 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            // 3-byte sequence (most Bangla characters are here)
            if (i + 2 >= len) return false;
            unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
            unsigned char b2 = static_cast<unsigned char>(bytes[i + 2]);
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;
            codepoint = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            seq_len = 3;
        } else {
            // 4-byte sequences, overlong encodings, etc.: not Bangla
            return false;
        }

        // Bangla Unicode block: U+0980..U+09FF
        // Also allow ZWNJ (U+200C) and ZWJ (U+200D) used as joiners in complex
        // Bangla conjuncts. No other non-Bangla codepoints are permitted.
        bool in_bangla_block = (codepoint >= 0x0980 && codepoint <= 0x09FF);
        bool is_joiner       = (codepoint == 0x200C || codepoint == 0x200D);

        if (!in_bangla_block && !is_joiner) {
            return false;
        }

        i += seq_len;
    }

    return true;
}

// ---------------------------------------------------------------------------
// HashRomanKey — FNV-1a hash of the roman key bytes
// ---------------------------------------------------------------------------

uint32_t GlobalModelReader::HashRomanKey(const char* key, size_t len) const {
    // FNV-1a 32-bit hash: fast, well-distributed, no division
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint8_t>(key[i]);
        hash *= 16777619u;
    }
    return hash;
}

// ---------------------------------------------------------------------------
// CloseHandles — release OS file mapping resources
// ---------------------------------------------------------------------------

void GlobalModelReader::CloseHandles() {
#ifdef _WIN32
    if (mapped_view_) {
        UnmapViewOfFile(mapped_view_);
        mapped_view_ = nullptr;
    }
    if (hMapping_) {
        CloseHandle(hMapping_);
        hMapping_ = nullptr;
    }
    if (hFile_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
#else
    if (mapped_view_ && file_size_ > 0) {
        ::munmap(const_cast<void*>(mapped_view_), file_size_);
        mapped_view_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

} // namespace bangla
