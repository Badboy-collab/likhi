// engine/src/global_model/global_model_reader.h
// Phase 3.1 — Global Model Reader
//
// Read-only, memory-mapped reader for global_model.bin (LGM1 format).
// ZERO network capability. ZERO writes. ZERO side-effects on any user data.
// If the model is absent, corrupt, or incompatible, IsLoaded() returns false
// and all lookups return 0.0f. Typing is never interrupted.
//
// Binary format (LGM1):
//   [0..3]   magic   'L','G','M','1'
//   [4..7]   schema_version      uint32_t  (must be 1)
//   [8..11]  global_model_version uint32_t  (monotonically increasing build #)
//   [12..15] min_client_major    uint16_t
//   [16..17] min_client_minor    uint16_t
//   [18..21] entry_count         uint32_t
//   [22..25] bucket_count        uint32_t
//   [26..29] string_pool_offset  uint32_t  (bytes from start of file)
//   [30..33] string_pool_size    uint32_t
//   [34..37] bucket_table_offset uint32_t
//   [38..41] entry_table_offset  uint32_t
//   [42..45] reserved            uint32_t  (must be 0)
//   [46..47] reserved2           uint16_t  (must be 0)
//   --- HEADER ENDS AT BYTE 48 ---
//   bucket_table: entry_count uint32_t indices (first entry index per bucket, 0xFFFFFFFF=empty)
//   entry_table: packed ModelEntry records
//   string_pool: raw UTF-8 strings (roman_key and bengali_word packed consecutively)
//
// Each ModelEntry:
//   uint32_t roman_offset;      // offset into string_pool
//   uint16_t roman_len;         // byte length of roman_key (max 24)
//   uint32_t bengali_offset;    // offset into string_pool
//   uint16_t bengali_len;       // byte length of bengali_word (max 120 = 40 codepoints * 3)
//   uint16_t frequency_score;   // crowd-scaled score, 0..10000 -> 0.0f..1.0f
//   uint32_t next_entry;        // next ModelEntry index in same bucket (0xFFFFFFFF = end)

#ifndef LIKHI_GLOBAL_MODEL_READER_H
#define LIKHI_GLOBAL_MODEL_READER_H

#include <string>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace bangla {

// Maximum field sizes enforced during validation
constexpr uint16_t kMaxRomanKeyBytes    = 24;
constexpr uint16_t kMaxBengaliBytes     = 120;   // 40 codepoints * max 3 UTF-8 bytes
constexpr uint32_t kMaxModelEntries     = 500000; // safety cap
constexpr uint32_t kMaxBucketCount      = 1048576; // 1M buckets max
constexpr uint32_t kMaxStringPoolBytes  = 64 * 1024 * 1024; // 64 MB cap

// LGM1 header layout — 46 bytes packed (no trailing padding with pack(1))
// Field layout:
//   magic[4] + schema_version(4) + global_model_version(4)
//   + min_client_major(2) + min_client_minor(2)
//   + entry_count(4) + bucket_count(4)
//   + string_pool_offset(4) + string_pool_size(4)
//   + bucket_table_offset(4) + entry_table_offset(4)
//   + reserved(4) + reserved2(2) = 50 bytes
#pragma pack(push, 1)
struct Lgm1Header {
    char     magic[4];           // 'L','G','M','1'
    uint32_t schema_version;     // must be 1
    uint32_t global_model_version;
    uint16_t min_client_major;
    uint16_t min_client_minor;
    uint32_t entry_count;
    uint32_t bucket_count;
    uint32_t string_pool_offset;
    uint32_t string_pool_size;
    uint32_t bucket_table_offset;
    uint32_t entry_table_offset;
    uint32_t reserved;           // must be 0
    uint16_t reserved2;          // must be 0
};
// 4+4+4+2+2+4+4+4+4+4+4+4+2 = 46 bytes
static_assert(sizeof(Lgm1Header) == 46, "Lgm1Header size check");

// Per-entry record: 4+2+4+2+2+4 = 18 bytes packed
struct ModelEntry {
    uint32_t roman_offset;
    uint16_t roman_len;
    uint32_t bengali_offset;
    uint16_t bengali_len;
    uint16_t frequency_score;   // 0..10000 maps to 0.0f..1.0f
    uint32_t next_entry;        // 0xFFFFFFFF = end of chain
};
static_assert(sizeof(ModelEntry) == 18, "ModelEntry size check");
#pragma pack(pop)

// Reason codes for load failure (for test verification only, not user-facing)
enum class ModelLoadResult : int {
    kOK                   = 0,
    kFileNotFound         = 1,
    kFileEmpty            = 2,
    kFileTooSmall         = 3,
    kMagicMismatch        = 4,
    kSchemaUnsupported    = 5,
    kClientVersionTooOld  = 6,
    kEntryCountExceedsMax = 7,
    kBucketCountExceedsMax= 8,
    kStringPoolTooBig     = 9,
    kOffsetOutOfBounds    = 10,
    kEntryTableOutOfBounds= 11,
    kBucketTableOutOfBounds=12,
    kMapFailed            = 13,
    kReservedNonZero      = 14,
    kInvalidEntry         = 15,  // invalid offsets/lengths inside an entry
};

class GlobalModelReader {
public:
    GlobalModelReader();
    ~GlobalModelReader();

    // Non-copyable, non-moveable: owns OS file mapping handles.
    GlobalModelReader(const GlobalModelReader&) = delete;
    GlobalModelReader& operator=(const GlobalModelReader&) = delete;

    // Open and validate a global_model.bin file.
    // Returns kOK only if all header, bounds, and structural checks pass.
    // On any failure: sets loaded_=false, closes all handles, returns error code.
    // NEVER modifies the file. NEVER throws.
    ModelLoadResult Open(const std::string& path);

    // Close the mapped file and reset state. Safe to call multiple times.
    void Close();

    // Returns true only if Open() returned kOK and Close() has not been called.
    bool IsLoaded() const;

    // Returns the crowd-derived score [0.0, 1.0] for the given pair.
    // If not loaded, or pair not found: returns 0.0f immediately.
    // O(1) hash-bucket lookup. No allocations. No blocking.
    // Max latency budget: 2 microseconds on modern hardware.
    float GetCandidateScore(const std::string& roman_key,
                            const std::string& bengali_word) const;

    // Model version for compatibility logging (0 if not loaded).
    uint32_t GetModelVersion() const;

    // Number of entries in the model (0 if not loaded).
    uint32_t GetEntryCount() const;

    // Last result code from Open() (for diagnostic/test purposes).
    ModelLoadResult GetLastLoadResult() const;

private:
    bool loaded_ = false;
    mutable ModelLoadResult last_result_ = ModelLoadResult::kFileNotFound;

#ifdef _WIN32
    HANDLE hFile_    = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
    const void* mapped_view_ = nullptr;
#else
    int      fd_     = -1;
    void*    mapped_view_ = nullptr;
#endif
    size_t   file_size_ = 0;

    // Cached pointers into the mapped view (all const — no writes possible)
    const Lgm1Header*   header_         = nullptr;
    const uint32_t*     bucket_table_   = nullptr;
    const ModelEntry*   entry_table_    = nullptr;
    const char*         string_pool_    = nullptr;

    // Validated field copies from header
    uint32_t entry_count_   = 0;
    uint32_t bucket_count_  = 0;
    uint32_t sp_size_       = 0;  // string pool size in bytes

    // Internal helpers
    void CloseHandles();
    bool ValidateHeader(const Lgm1Header* h, size_t file_size) const;
    bool ValidateAllEntries() const;
    bool IsValidBengaliUtf8(const char* bytes, uint16_t len) const;
    bool IsValidRomanKey(const char* bytes, uint16_t len) const;
    uint32_t HashRomanKey(const char* key, size_t len) const;
};

} // namespace bangla

#endif // LIKHI_GLOBAL_MODEL_READER_H
