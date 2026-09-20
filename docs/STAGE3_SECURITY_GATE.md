# LIKHI — STAGE 3 SECURITY GATE SPECIFICATION
## Mandatory Security and Privacy Gates for Every Phase

**Document Version:** 1.0.0
**Status:** SPECIFICATION — Active Throughout Stage 3 Implementation
**Date:** 2026-09-09

---

> [!IMPORTANT]
> Every gate in this document is a **hard stop**.
> A phase does not proceed until ALL gates in that phase are GREEN.
> No exceptions. No waivers. No "we'll fix it in the next phase."

---

## SECTION 1 — REGRESSION GATE (All Phases)

**Status required before starting any phase:** 881/881 PASS

```powershell
# Run after every code change before proceeding
cd engine\build
cmake --build . --config Release
ctest --output-on-failure
```

**Required output:**
```
Test project .../engine/build
    881 tests passed, 0 tests failed
```

If any test fails: **STOP. Do not proceed. Identify regression. Fix and re-verify.**

---

## SECTION 2 — PAYLOAD FIELD ALLOWLIST/DENYLIST GATE

**Applies to:** Phase 3.2 (Local Staging Queue), Phase 3.5 (Upload), Phase 3.15 (Privacy Testing)

### 2.1 Outgoing Payload Test

This automated test must exist before any upload code is written and must run in CI:

```cpp
// tests/privacy/test_payload_fields.cpp
// Intercepts the outgoing contribution batch and verifies field allowlist.

TEST(PayloadPrivacy, OnlyAllowedFieldsPresent) {
    // Arrange: enable global contribution, type a word, commit
    FakeConsentProvider consent(/* global_contribution=*/true);
    FakeLexicon lexicon;  // contains "ami" -> "আমি" with rank 500
    FakePersonalDict dict;
    dict.AddWord("ami", L"আমি", /*frequency=*/5);  // passes R3

    StagingQueue queue(test_path_, &consent);
    queue.Initialize();

    // Act: append a tuple
    queue.TryAppend("ami", L"আমি", 1, "2026-W37");

    // Assert: read back the staged record and verify fields
    StagedRecord rec = ReadLastRecord(test_path_);

    // ALLOWLIST: only these fields are permitted
    EXPECT_TRUE(rec.HasField("roman_key"));
    EXPECT_TRUE(rec.HasField("selected_bengali"));
    EXPECT_TRUE(rec.HasField("vote_weight"));
    EXPECT_TRUE(rec.HasField("week_bucket"));

    // DENYLIST: none of these must appear
    EXPECT_FALSE(rec.HasField("timestamp"));
    EXPECT_FALSE(rec.HasField("millisecond"));
    EXPECT_FALSE(rec.HasField("device_id"));
    EXPECT_FALSE(rec.HasField("session_id"));
    EXPECT_FALSE(rec.HasField("ip_address"));
    EXPECT_FALSE(rec.HasField("machine_name"));
    EXPECT_FALSE(rec.HasField("username"));
    EXPECT_FALSE(rec.HasField("window_title"));
    EXPECT_FALSE(rec.HasField("document_text"));
    EXPECT_FALSE(rec.HasField("clipboard"));
    EXPECT_FALSE(rec.HasField("full_sentence"));
    EXPECT_FALSE(rec.HasField("rejected_candidate"));
    EXPECT_FALSE(rec.HasField("unselected_candidate"));
    EXPECT_FALSE(rec.HasField("password"));
    EXPECT_FALSE(rec.HasField("email"));
    EXPECT_FALSE(rec.HasField("url"));
    EXPECT_FALSE(rec.HasField("keystroke_timing"));
    EXPECT_FALSE(rec.HasField("frequency"));        // personal frequency NOT exported
    EXPECT_FALSE(rec.HasField("last_used"));        // personal timestamp NOT exported
    EXPECT_FALSE(rec.HasField("personal_score"));   // internal score NOT exported
}
```

### 2.2 Upload JSON Payload Test

Before any network upload is implemented:

```cpp
// tests/privacy/test_upload_payload.cpp
TEST(PayloadPrivacy, HttpBodyContainsOnlyAllowedFields) {
    // Intercept the JSON body before it is sent (mock HTTP client)
    // Parse the JSON
    // Verify field names match allowlist exactly
    // Verify no nested fields contain any denylist strings

    std::string body = captured_http_body_;
    auto json = ParseJson(body);

    for (const auto& tuple : json["contributions"]) {
        // Allowlist check
        for (const auto& [key, _] : tuple.items()) {
            EXPECT_TRUE(IsInAllowlist(key))
                << "Unexpected field in payload: " << key;
        }
        // Type checks
        EXPECT_TRUE(tuple["roman_key"].is_string());
        EXPECT_TRUE(tuple["selected_bengali"].is_string());
        EXPECT_EQ(tuple["vote_weight"].get<int>(), 1);
        EXPECT_TRUE(tuple["week_bucket"].is_string());

        // Denylist substring check on values
        std::string roman = tuple["roman_key"];
        EXPECT_LE(roman.size(), 24u);
        EXPECT_TRUE(IsAllLowerAlpha(roman));
    }
}
```

**This test must be written BEFORE network upload is implemented. If this test cannot be
written yet (no upload code exists), this gate is PENDING (not a failure). Once upload
code exists, this test must immediately be added and must pass.**

---

## SECTION 3 — TSF NETWORK ISOLATION GATE

**Applies to:** All phases. Permanent.

### 3.1 Import Table Check

```powershell
# Run against bangla_tsf.dll after every build
# Expected: PASS. Any network DLL = BUILD FAILURE.

$allowed = @(
    "kernel32.dll",
    "user32.dll",
    "advapi32.dll",
    "ole32.dll",
    "msctf.dll",
    "ntdll.dll"
)

$imports = dumpbin /imports bangla_tsf.dll |
           Select-String "\.dll" |
           ForEach-Object { $_.Line.Trim().ToLower() }

$network_dlls = @(
    "ws2_32.dll",
    "wininet.dll",
    "winhttp.dll",
    "urlmon.dll",
    "dnsapi.dll",
    "mswsock.dll",
    "secur32.dll"
)

$violations = $imports | Where-Object {
    $name = $_
    $network_dlls | Where-Object { $name -like "*$_*" }
}

if ($violations) {
    Write-Error "FAIL: bangla_tsf.dll imports network DLL: $violations"
    exit 1
} else {
    Write-Host "PASS: bangla_tsf.dll has zero network imports."
}
```

### 3.2 DEV Key Segregation Check

```powershell
# Run against production-target binary before release signing
# Must PASS before any model is signed with the production key

python tools/build/check_key_segregation.py bangla_tsf.dll
python tools/build/check_key_segregation.py likhi_sync.exe
```

---

## SECTION 4 — MODEL VERIFICATION GATE

**Applies to:** Phase 3.6, Phase 3.7, Phase 3.13, Phase 3.14, Phase 3.16

### 4.1 Required Test Cases

All of the following tests must exist and pass:

```
TEST GROUP: model_signature_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                          │ Expected Result                   │
├────────────────────────────────────┼───────────────────────────────────┤
│ ValidSigValidHash                  │ kOK                               │
│ ValidSigWrongHash                  │ kHashMismatch, .tmp deleted       │
│ WrongSigValidHash                  │ kSignatureInvalid, .tmp deleted   │
│ WrongMagicBytes                    │ kMagicMismatch, .tmp deleted      │
│ OldModelVersion                    │ kVersionTooOld, .tmp deleted      │
│ FutureClientVersionRequired        │ kVersionTooNew, .tmp deleted      │
│ DevLabelInProductionBuild          │ kDevKeyInProduction, .tmp deleted │
│ ZeroByteFile                       │ kReadError, .tmp deleted          │
│ TruncatedFile                      │ kReadError or kHashMismatch       │
│ CorruptMiddleOfFile                │ kHashMismatch, .tmp deleted       │
│ AllowedByDevKeyInDevBuild          │ kOK (dev build only)              │
│ TypingContinuesDuringVerifyFail    │ Engine remains functional         │
│ TypingContinuesDuringInstall       │ Engine remains functional         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Rollback Test Cases

```
TEST GROUP: model_rollback_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                          │ Expected Result                   │
├────────────────────────────────────┼───────────────────────────────────┤
│ SuccessfulInstall                  │ .bin updated, old .bin = new .bak │
│ InstallFailsMidSwap                │ .bak restored as active           │
│ RollbackSuccess                    │ .bak becomes new .bin             │
│ CorruptBinFallsBackToBak           │ .bak loaded, typing continues     │
│ CorruptBinCorruptBakFallsToLexicon │ IsLoaded=false, typing continues  │
│ BakOlderThan30Days                 │ .bak cleaned up by cleanup worker │
│ RevocationNotice                   │ .bin and .bak deleted, lexicon used│
│ RevocationThenDownloadNew          │ new .bin downloaded and installed  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## SECTION 5 — CONSENT GATE

**Applies to:** Phase 3.3, Phase 3.4, Phase 3.13, Phase 3.15

### 5.1 Consent Test Cases

```
TEST GROUP: consent_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                          │ Expected Result                   │
├────────────────────────────────────┼───────────────────────────────────┤
│ GlobalContributionDefaultIsOff     │ Registry key = 0 on clean install │
│ ModelDownloadDefaultIsOff          │ Registry key = 0 on clean install │
│ PersonalLearningDefaultIsOn        │ Registry key = 1 on clean install │
│ TurningOffContributionStopsAppend  │ No new records in staged_learning │
│ RevocationDeletesStagingFile       │ staged_learning.dat deleted ≤10min│
│ RevocationPreservesPersonalDict    │ user_dict.txt unchanged           │
│ RevocationAbortsInFlightUpload     │ HTTP POST cancelled               │
│ ClearPersonalDataDeletesUserDict   │ user_dict.txt deleted             │
│ ClearPersonalDataPreservesConsent  │ consent flags unchanged           │
│ IndependentSettingsNoInterference  │ Each setting affects only itself  │
│ ContributionOffNeverUploads        │ No HTTP calls in sync worker      │
│ ModelDownloadOffNeverDownloads     │ No HTTP calls for version check   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## SECTION 6 — RARITY GATE TESTS

**Applies to:** Phase 3.2, Phase 3.15

```
TEST GROUP: rarity_gate_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                              │ Expected Result               │
├────────────────────────────────────────┼───────────────────────────────┤
│ CommonWordPassesAllRules               │ Appended to staging queue     │
│ WordNotInLexiconFailsR1                │ Not staged                    │
│ RareWordLowRankFailsR1                 │ Not staged                    │
│ RomanKeyNotInPhoneticIndexFailsR2      │ Not staged                    │
│ LocalFrequencyBelow5FailsR3            │ Not staged (freq=4)           │
│ LocalFrequencyExactly5PassesR3         │ Staged (freq=5)               │
│ ManualCustomFlagFailsR4                │ Not staged                    │
│ ContributionExcludedFlagFailsR4        │ Not staged                    │
│ PersonalNameNotInLexiconFailsR1        │ Not staged (rare surname)     │
│ PersonalNameInLexiconCaughtByModeration│ Staged but moderation rejects │
│ CustomShortcutFailsR1OrR2              │ Not staged                    │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## SECTION 7 — OFFLINE OPERATION GATE

**Applies to:** Phase 3.16, and must not regress in any phase.

```
TEST GROUP: offline_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                              │ Expected Result               │
├────────────────────────────────────────┼───────────────────────────────┤
│ NoGlobalModelOnDisk                    │ Engine starts, lexicon used   │
│ CorruptGlobalModel                     │ Engine starts, lexicon used   │
│ InvalidSignatureGlobalModel            │ Engine starts, lexicon used   │
│ NetworkUnavailableAtSyncTime           │ Sync worker exits cleanly     │
│ ServerReturns500                       │ Backoff, retry, then exit     │
│ ServerTimeoutDuringDownload            │ .tmp deleted, current retained│
│ DNSFailure                             │ Sync exits, no crash, no block│
│ OfflineTypingFull881RegressionSuite    │ 881/881 PASS                  │
│ StagingQueueBuildsDuringOffline        │ Records accumulate locally    │
│ StagingQueueFlushedWhenOnlineReturns   │ Upload succeeds after reconnect│
│ StagingRecordsOlderThan7DaysDiscarded  │ Pruned before upload          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## SECTION 8 — SERVER VALIDATION GATE

**Applies to:** Phase 3.8, Phase 3.9, Phase 3.10

```
TEST GROUP: server_ingestion_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                              │ Expected Result               │
├────────────────────────────────────────┼───────────────────────────────┤
│ ValidPayloadAccepted                   │ HTTP 200, accepted=N          │
│ InvalidPoWRejected                     │ HTTP 400                      │
│ NonBanglaUnicodeRejected               │ HTTP 400                      │
│ VoteWeightNot1Rejected                 │ HTTP 400                      │
│ FutureWeekBucketRejected               │ HTTP 400                      │
│ WeekBucketOver7DaysAgoRejected         │ HTTP 400                      │
│ OversizePayloadRejected                │ HTTP 413                      │
│ Over20TuplesRejected                   │ HTTP 400                      │
│ RateLimitExceeded                      │ HTTP 429                      │
│ IPNotInApplicationDatabase             │ DB audit: no IP field exists  │
│ IPNotInLogFile                         │ Log file: no IP in entries    │
│ DuplicateSameSourceSameDay             │ Count incremented only once   │
│ KThresholdNotReachedNoModQueue         │ Not in moderation queue       │
│ KThresholdReachedEntersModQueue        │ In moderation queue           │
│ 90DayExpiredPurged                     │ Record deleted from store     │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## SECTION 9 — GLOBAL MODEL PRECEDENCE GATE

**Applies to:** Phase 3.1, Phase 3.13

### 9.1 Invariant Tests

```
TEST GROUP: model_precedence_tests

┌─────────────────────────────────────────────────────────────────────────┐
│ TEST NAME                              │ Expected Result               │
├────────────────────────────────────────┼───────────────────────────────┤
│ PinnedWordBeatGlobalModel              │ Pinned candidate = #1         │
│ PersonalFreq3BeatGlobalModel           │ Personal candidate = #1       │
│ PersonalFreq2DoesNotPinOverGlobal      │ Personal boosted but not pinned│
│ GlobalModelUpdateDoesNotModifyUserDict │ user_dict.txt byte-identical  │
│ GlobalModelRollbackDoesNotModifyUserDict│ user_dict.txt byte-identical │
│ LexiconUpdateDoesNotModifyUserDict     │ user_dict.txt byte-identical  │
│ AppUpgradeDoesNotModifyUserDict        │ user_dict.txt byte-identical  │
│ AppUninstallDoesNotDeleteUserDict      │ user_dict.txt still present   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## SECTION 10 — STOP CONDITIONS

Implementation MUST STOP immediately if any of the following are observed:

| Condition | Stop Reason |
| :--- | :--- |
| Any of the 881 existing tests fail | Stage 1/2 regression |
| `bangla_tsf.dll` imports any network DLL | TSF network isolation violated |
| Staging record contains a timestamp field | Timing data in contribution |
| Staging record contains frequency field | Personal score leaking |
| Staging record contains rejected_candidates | Selection context leaking |
| Upload occurs when GlobalContribution = 0 | Consent bypass |
| user_dict.txt is modified by model update | Precedence invariant violated |
| Model loaded without Ed25519 verification | Security bypass |
| Model loaded without SHA-256 verification | Integrity bypass |
| Private key appears in any committed file | Key security violated |
| CI key appears in a production binary | Key segregation violated |
| IP address found in server application DB | Privacy violation |
| Typing blocks waiting for network response | Latency invariant violated |

**On detection of any stop condition:**
```
1. STOP all implementation work immediately.
2. Identify the exact root cause.
3. Write a report describing the root cause and proposed fix.
4. Wait for explicit approval before resuming.
```

---

## SECTION 11 — PHASE GATE SUMMARY

| Phase | Primary Gate | Gate Spec |
| :---: | :--- | :--- |
| 3.1 | No network imports in TSF, 881 tests pass | §1, §3 |
| 3.2 | Payload field allowlist/denylist test | §1, §2, §3, §6 |
| 3.3 | Consent defaults, independent settings | §1, §5 |
| 3.4 | Consent revocation deletes queue | §1, §5 |
| 3.5 | TLS 1.3, cert pinning, TSF unchanged | §1, §3 |
| 3.6 | All model verification cases pass | §1, §4.1 |
| 3.7 | All rollback cases pass | §1, §4.2, §9 |
| 3.8 | IP not in DB, PoW, field validation | §1, §8 |
| 3.9 | No raw records after aggregation | §1, §8 |
| 3.10 | K-threshold, 7-day, subnet diversity | §1, §8 |
| 3.11 | Audit log, no direct publish | §1 |
| 3.12 | Model signed with CI key, verifies | §1, §4.1 |
| 3.13 | Full E2E scenario, consent mid-flight | §1–§9 all |
| 3.14 | All adversarial tests from §4, §5 | §4, §5, §6 |
| 3.15 | Payload field test, privacy tests | §2, §5, §6 |
| 3.16 | 881 regression, all offline cases | §1, §7 |

---

## SECTION 12 — RELEASE SAFETY CHECKLIST

No production global model distribution until ALL of the following are GREEN:

| # | Gate | Status |
|:--|:---|:---:|
| R1 | Signing key custody procedure approved (STAGE3_SIGNING_KEY_CUSTODY.md) | ⬜ |
| R2 | Production key generated in physical signing ceremony | ⬜ |
| R3 | Production key embedded in production binary, all-zeros removed | ⬜ |
| R4 | Key segregation CI check passing | ⬜ |
| R5 | 881/881 regression tests pass | ⬜ |
| R6 | Payload field allowlist/denylist tests pass | ⬜ |
| R7 | TSF import table has zero network DLLs (verified post-build) | ⬜ |
| R8 | Consent defaults verified: Global Contribution = OFF | ⬜ |
| R9 | Rollback tested: all 8 rollback scenarios pass | ⬜ |
| R10 | Offline mode tested: all 11 offline scenarios pass | ⬜ |
| R11 | Server-side IP stripping verified by log inspection | ⬜ |
| R12 | Human moderation workflow is operational | ⬜ |
| R13 | Privacy testing (Phase 3.15) complete | ⬜ |
| R14 | Security testing (Phase 3.14) complete | ⬜ |

**If any item is ⬜, production model distribution is BLOCKED.**
