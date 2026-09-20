// tools/build_test_global_model.cpp
// Phase 3.1 — Test Global Model Binary Builder
//
// Produces valid and deliberately-malformed global_model.bin files
// for use by the Phase 3.1 test suite.
// THIS IS A BUILD/TEST TOOL ONLY. Not part of the production engine.
// Uses CI/test data only. Contains no production signing key.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <functional>

#include "../engine/src/global_model/global_model_reader.h"

using namespace bangla;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static uint32_t FNV1a32(const char* key, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint8_t>(key[i]);
        h *= 16777619u;
    }
    return h;
}

// Writes a complete, valid LGM1 binary with the given entries.
// entries: vector of (roman_key, bengali_word, frequency_score_0_to_10000)
struct Lgm1Options {
    uint32_t schema_version_override = 1;
    uint32_t model_version_override  = 10001;
    bool     corrupt_magic           = false;
    bool     corrupt_reserved        = false;
    bool     bad_sp_offset           = false;
    bool     bad_bucket_offset       = false;
    bool     bad_entry_offset        = false;
    bool     bad_roman_key           = false;
    bool     bad_bengali             = false;
    bool     oversized_entry         = false;
    bool     duplicate_entries       = false;
};

// Writes a complete, valid LGM1 binary with the given entries.
// entries: vector of (roman_key, bengali_word, frequency_score_0_to_10000)
static bool WriteLgm1(
    const std::string& path,
    const std::vector<std::tuple<std::string, std::string, uint16_t>>& entries,
    const Lgm1Options& opt = {}
) {
    uint32_t schema_version_override = opt.schema_version_override;
    uint32_t model_version_override  = opt.model_version_override;
    bool corrupt_magic           = opt.corrupt_magic;
    bool corrupt_reserved        = opt.corrupt_reserved;
    bool bad_sp_offset           = opt.bad_sp_offset;
    bool bad_bucket_offset       = opt.bad_bucket_offset;
    bool bad_entry_offset        = opt.bad_entry_offset;
    bool bad_roman_key           = opt.bad_roman_key;
    bool bad_bengali             = opt.bad_bengali;
    bool oversized_entry         = opt.oversized_entry;
    bool duplicate_entries       = opt.duplicate_entries;

    const uint32_t N = static_cast<uint32_t>(entries.size());
    // Use a fixed bucket count large enough to minimise collisions
    const uint32_t B = (N == 0) ? 16 : N * 4;

    // Build string pool
    std::string sp;
    struct EntryData {
        uint32_t roman_off, bengali_off;
        uint16_t roman_len, bengali_len;
        uint16_t score;
        uint32_t chain;
    };
    std::vector<EntryData> edata(N);
    std::vector<uint32_t>  bucket_heads(B, 0xFFFFFFFF);

    for (uint32_t i = 0; i < N; ++i) {
        const auto& [roman, bengali, score] = entries[i];

        std::string actual_roman  = roman;
        std::string actual_bengali = bengali;

        if (bad_roman_key)   actual_roman  = "BAD123!";    // fails IsValidRomanKey
        if (bad_bengali)     actual_bengali = "not_bengali"; // fails IsValidBengaliUtf8
        if (oversized_entry) actual_roman  = std::string(kMaxRomanKeyBytes + 1, 'a');

        edata[i].roman_off  = static_cast<uint32_t>(sp.size());
        edata[i].roman_len  = static_cast<uint16_t>(actual_roman.size());
        sp.append(actual_roman);

        edata[i].bengali_off = static_cast<uint32_t>(sp.size());
        edata[i].bengali_len = static_cast<uint16_t>(actual_bengali.size());
        sp.append(actual_bengali);

        edata[i].score = score;
        edata[i].chain = 0xFFFFFFFF;

        // Insert into bucket chain
        uint32_t bucket_idx = FNV1a32(actual_roman.c_str(), actual_roman.size()) % B;
        uint32_t prev = bucket_heads[bucket_idx];
        if (prev == 0xFFFFFFFF) {
            bucket_heads[bucket_idx] = i;
        } else {
            // Walk to end of chain
            uint32_t cur = prev;
            while (edata[cur].chain != 0xFFFFFFFF) cur = edata[cur].chain;
            edata[cur].chain = i;
        }
    }

    // If duplicate_entries requested, add a duplicate of entry 0 (if exists)
    if (duplicate_entries && N > 0) {
        EntryData dup = edata[0];
        // In the test binary, duplicates are flagged by writing entry[0] twice
        // This exercises the scanner but is NOT expected to crash.
        // The reader must handle duplicates gracefully (returns first match score).
        edata.push_back(dup);
    }

    uint32_t actual_N = static_cast<uint32_t>(edata.size());

    // Build header
    Lgm1Header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic[0] = corrupt_magic ? 'X' : 'L';
    hdr.magic[1] = 'G';
    hdr.magic[2] = 'M';
    hdr.magic[3] = '1';
    hdr.schema_version        = schema_version_override;
    hdr.global_model_version  = model_version_override;
    hdr.min_client_major      = 1;
    hdr.min_client_minor      = 0;
    hdr.entry_count           = actual_N;
    hdr.bucket_count          = B;
    hdr.reserved              = corrupt_reserved ? 0xDEADBEEF : 0;
    hdr.reserved2             = 0;

    // Layout: [header][bucket_table][entry_table][string_pool]
    uint32_t header_end     = sizeof(Lgm1Header);
    uint32_t bucket_off     = header_end;
    uint32_t entry_off      = bucket_off + B * sizeof(uint32_t);
    uint32_t sp_off         = entry_off  + actual_N * sizeof(ModelEntry);

    hdr.bucket_table_offset  = bad_bucket_offset ? 0x00000001 : bucket_off;  // 1 = misaligned/invalid
    hdr.entry_table_offset   = bad_entry_offset  ? 0xFFFFFFF0 : entry_off;
    hdr.string_pool_offset   = bad_sp_offset     ? 0xFFFFFFF0 : sp_off;
    hdr.string_pool_size     = static_cast<uint32_t>(sp.size());

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cerr << "ERROR: Cannot open output: " << path << "\n";
        return false;
    }

    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(bucket_heads.data()), B * sizeof(uint32_t));

    for (const auto& e : edata) {
        ModelEntry me;
        me.roman_offset   = e.roman_off;
        me.roman_len      = e.roman_len;
        me.bengali_offset = e.bengali_off;
        me.bengali_len    = e.bengali_len;
        me.frequency_score = e.score;
        me.next_entry     = e.chain;
        f.write(reinterpret_cast<const char*>(&me), sizeof(me));
    }

    f.write(sp.c_str(), static_cast<std::streamsize>(sp.size()));
    f.close();
    return true;
}

// -----------------------------------------------------------------------
// Main — build all test fixtures
// -----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::string out_dir = ".";
    if (argc >= 2) out_dir = argv[1];

    auto p = [&](const std::string& name) { return out_dir + "/" + name; };

    // Common test entries with valid Bengali (পরিষ্কার, আমি, ভালো)
    // Bengali UTF-8 bytes embedded as string literals
    // ami -> আমি  (U+0986 U+09AE U+09BF)
    // valo -> ভালো (U+09AD U+09BE U+09B2 U+09CB)
    const std::string ami_bengali    = "\xe0\xa6\x86\xe0\xa6\xae\xe0\xa6\xbf";        // আমি
    const std::string valo_bengali   = "\xe0\xa6\xad\xe0\xa6\xbe\xe0\xa6\xb2\xe0\xa7\x8b"; // ভালো
    const std::string bhalo_bengali  = "\xe0\xa6\xad\xe0\xa6\xbe\xe0\xa6\xb2\xe0\xa7\x8b"; // same: ভালো
    const std::string bangla_bengali = "\xe0\xa6\xac\xe0\xa6\xbe\xe0\xa6\x82\xe0\xa6\xb2\xe0\xa6\xbe"; // বাংলা

    using E3 = std::tuple<std::string, std::string, uint16_t>;
    std::vector<E3> valid_entries = {
        {"ami",     ami_bengali,    8500},
        {"valo",    valo_bengali,   9000},
        {"bhalo",   bhalo_bengali,  8800},
        {"bangla",  bangla_bengali, 9500},
    };

    // 1. Valid model
    WriteLgm1(p("gm_valid.bin"), valid_entries);
    std::cout << "OK gm_valid.bin\n";

    // 2. Missing model — just don't create gm_missing.bin (test opens it expecting failure)
    std::cout << "OK gm_missing.bin (not created — absence is the test)\n";

    // 3. Empty file
    { std::ofstream f(p("gm_empty.bin"), std::ios::binary | std::ios::trunc); }
    std::cout << "OK gm_empty.bin\n";

    // 4. Truncated file (partial header only)
    {
        std::ofstream f(p("gm_truncated.bin"), std::ios::binary | std::ios::trunc);
        Lgm1Header hdr;
        memset(&hdr, 0, sizeof(hdr));
        memcpy(hdr.magic, "LGM1", 4);
        f.write(reinterpret_cast<const char*>(&hdr), 10); // partial write
    }
    std::cout << "OK gm_truncated.bin\n";

    // 5. Corrupted header (first byte changed)
    {
        std::ofstream f(p("gm_corrupt_header.bin"), std::ios::binary | std::ios::trunc);
        Lgm1Header hdr;
        memset(&hdr, 0, sizeof(hdr));
        memcpy(hdr.magic, "LGM1", 4);
        hdr.schema_version = 1;
        // Corrupt: set entry_count to huge number
        hdr.entry_count = 0xFFFFFFFF;
        f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    }
    std::cout << "OK gm_corrupt_header.bin\n";

    // 6. Wrong magic
    WriteLgm1(p("gm_wrong_magic.bin"), valid_entries, Lgm1Options{ .corrupt_magic = true });
    std::cout << "OK gm_wrong_magic.bin\n";

    // 7. Unsupported schema version
    WriteLgm1(p("gm_bad_schema.bin"), valid_entries, Lgm1Options{ .schema_version_override = 99 });
    std::cout << "OK gm_bad_schema.bin\n";

    // 8. Reserved field non-zero
    WriteLgm1(p("gm_reserved_nonzero.bin"), valid_entries, Lgm1Options{ .corrupt_reserved = true });
    std::cout << "OK gm_reserved_nonzero.bin\n";

    // 9. Invalid string pool offset
    WriteLgm1(p("gm_bad_sp_offset.bin"), valid_entries, Lgm1Options{ .bad_sp_offset = true });
    std::cout << "OK gm_bad_sp_offset.bin\n";

    // 10. Invalid bucket table offset
    WriteLgm1(p("gm_bad_bucket_offset.bin"), valid_entries, Lgm1Options{ .bad_bucket_offset = true });
    std::cout << "OK gm_bad_bucket_offset.bin\n";

    // 11. Invalid entry table offset
    WriteLgm1(p("gm_bad_entry_offset.bin"), valid_entries, Lgm1Options{ .bad_entry_offset = true });
    std::cout << "OK gm_bad_entry_offset.bin\n";

    // 12. Invalid Roman key in entries
    WriteLgm1(p("gm_bad_roman.bin"), valid_entries, Lgm1Options{ .bad_roman_key = true });
    std::cout << "OK gm_bad_roman.bin\n";

    // 13. Invalid Bengali text in entries
    WriteLgm1(p("gm_bad_bengali.bin"), valid_entries, Lgm1Options{ .bad_bengali = true });
    std::cout << "OK gm_bad_bengali.bin\n";

    // 14. Oversized entry (roman key too long)
    WriteLgm1(p("gm_oversized_entry.bin"), valid_entries, Lgm1Options{ .oversized_entry = true });
    std::cout << "OK gm_oversized_entry.bin\n";

    // 15. Duplicate entries
    WriteLgm1(p("gm_duplicate.bin"), valid_entries, Lgm1Options{ .duplicate_entries = true });
    std::cout << "OK gm_duplicate.bin\n";


    std::cout << "All test fixtures built successfully.\n";
    return 0;
}
