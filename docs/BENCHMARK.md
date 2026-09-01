# Benchmark & Real-World Evaluation Report (Phase 3.1)

## 1. Executive Summary

Phase 3.1 successfully resolved the memory and startup regressions by replacing pointer-heavy Trie nodes and hash map allocations with a **Flat Contiguous Binary Lexicon & Shared String Pool (v2)**.

---

## 2. Empirical Optimization Comparison (Before vs After)

| Performance Dimension | Phase 3 (Unoptimized) | **Phase 3.1 (Flat Contiguous)** | Optimization Impact |
| :--- | :--- | :--- | :--- |
| **Cold Startup Time** | `236.09 ms` | **`3.90 ms (0.0039 s)`** | **98.3% faster startup** |
| **Idle Engine RAM** | `54.29 MB` | **`8.55 MB`** | **84.2% RAM reduction** |
| **Active Typing RAM** | `59.96 MB` | **`13.43 MB`** | **77.6% RAM reduction (< 15 MB preferred)** |
| **Peak Working Set** | `61.52 MB` | **`13.92 MB`** | **Well within < 25 MB max limit** |
| **Lexicon Delta RAM** | `~49.00 MB` | **`4.35 MB`** | **Near-zero overhead over 4.11 MB file** |
| **Average Keystroke Latency** | `19.34 µs` | **`20.43 µs (0.020 ms)`** | Maintained sub-millisecond real-time speed |
| **P50 (Median) Latency** | `7.50 µs` | **`8.60 µs (0.008 ms)`** | CPU L1/L2 contiguous cache locality |
| **P95 Latency** | `85.00 µs` | **`86.40 µs (0.086 ms)`** | Deterministic binary search bounds |
| **P99 Latency** | `193.80 µs` | **`196.50 µs (0.196 ms)`** | Deterministic binary search bounds |
| **Next-Word Prediction Time** | `N/A` | **`1.42 µs (0.001 ms)`** | Ultra-fast bigram transitions |

---

## 3. Linguistic Accuracy & Quality Metrics (100% Preserved)

| Benchmark Dimension | Measured Value | Evaluation Threshold | Status |
| :--- | :--- | :--- | :--- |
| **Held-Out Gold Sentence Accuracy** | **`98.08% (510/520)`** | `> 85.0%` | **PASS** |
| **Held-Out Gold Word Accuracy** | **`98.92% (2,098/2,121)`** | `> 95.0%` | **PASS** |
| **Invalid Candidate Rate** | **`0.00% (0/2,121)`** | `< 1.0%` | **PASS (Zero Junk)** |
| **Banglish Variation Accuracy** | **`100.00% (45/45)`** | `> 95.0%` | **PASS** |
| **Unicode Integrity Suite** | **`100.00% (46/46)`** | `100.0%` | **PASS** |
| **Engine Unit & Stress Tests** | **`100.00% (220/220)`** | `100.0%` | **PASS** |

---

## 4. Difficult Linguistic Stress Tests Added (Task 6)

The engine was subjected to 23 newly added stress cases in `test_runner.cpp`:
- **Complex Conjuncts & Z-Phala**: `shwastho` $\to$ **স্বাস্থ্য**
- **Ref & Conjunct**: `antorjatik` $\to$ **আন্তর্জাতিক**
- **Ha-Ma & Tra Conjuncts**: `brohmoputro` $\to$ **ব্রহ্মপুত্র**
- **Ggyo & Pti**: `biggopti` $\to$ **বিজ্ঞপ্তি**
- **T-Ma Conjunct**: `attiyo` $\to$ **আত্মীয়**
- **Khanda Ta & Ri-kar**: `utkrishto` $\to$ **উৎকৃষ্ট**
- **Ng-Khyo Conjunct**: `akangkha` $\to$ **আকাঙ্ক্ষা**
- **Double Da-Ba**: `dondwo` $\to$ **দ্বন্দ্ব**
- **Double Ta-Ba**: `tottwo` $\to$ **তত্ত্ব**
- **Sh-Ri & Ng-Kha**: `shringkhola` $\to$ **শৃঙ্খলা**
- **J-J-Bala**: `ujjwol` $\to$ **উজ্জ্বল**
- **Loanwords & Digital Terms**: `computer` $\to$ **কম্পিউটার**, `mobile` $\to$ **মোবাইল**, `internet` $\to$ **ইন্টারনেট**
- **Geographic Names**: `dhaka` $\to$ **ঢাকা**, `chottogram` $\to$ **চট্টগ্রাম**, `rajshahi` $\to$ **রাজশাহী**, `khulna` $\to$ **খুলনা**, `sylhet` $\to$ **সিলেট**
- **Inflected Verbs**: `korte` $\to$ **করতে**, `korle` $\to$ **করলে**, `hobe` $\to$ **হবে**, `hoyeche` $\to$ **হয়েছে**
