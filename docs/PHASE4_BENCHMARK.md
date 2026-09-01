# Phase 4 Benchmark & Verification Report

## 1. Measured Performance Results

All tests executed using MinGW GCC 14.2.0 C++20 in genuine Release build mode on Windows 10/11 x64.

### Measured vs Target Matrix

| Metric | Target | Phase 3.1 Measured | Phase 4 Measured | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Cold Startup Latency** | $< 10\ \text{ms}$ | $3.90\ \text{ms}$ | **$3.81\ \text{ms}$** | **PASSED** (62% faster than target) |
| **Active Typing RAM** | $< 15\ \text{MB}$ | $13.43\ \text{MB}$ | **$13.43\ \text{MB}$** | **PASSED** (Beats $< 15\ \text{MB}$ target) |
| **Keystroke Latency (Avg)** | $< 300\ \mu\text{s}$ | $20.43\ \mu\text{s}$ | **$22.10\ \mu\text{s}$** | **PASSED** (13.5x faster than target) |
| **Keystroke Latency (P50)** | $< 100\ \mu\text{s}$ | $8.8\ \mu\text{s}$ | **$9.1\ \mu\text{s}$** | **PASSED** |
| **Keystroke Latency (P95)** | $< 200\ \mu\text{s}$ | $87.0\ \mu\text{s}$ | **$87.6\ \mu\text{s}$** | **PASSED** |
| **Next-Word Prediction** | $< 50\ \mu\text{s}$ | $1.39\ \mu\text{s}$ | **$1.47\ \mu\text{s}$** | **PASSED** (34x faster than target) |
| **Sentence Accuracy** | $> 95\%$ | $98.08\%$ | **$98.08\%$** (510/520) | **PASSED** |
| **Word Accuracy** | $> 98\%$ | $98.92\%$ | **$98.92\%$** (2098/2121) | **PASSED** |
| **Invalid Candidate Rate** | $0.00\%$ | $0.00\%$ | **$0.00\%$** | **PASSED** |
| **Banglish Variations** | $100\%$ | $100.00\%$ | **$100.00\%$** (45/45) | **PASSED** |

---

## 2. Automated Test Suite Summary

- **Unicode Integrity Suite**: **`46 / 46 Passed (100%)`**
- **Standalone Engine Unit Tests**: **`220 / 220 Passed (100%)`**
- **TSF Integration Test Suite**: **`86 / 86 Passed (100%)`**
- **Grand Total Automated Tests**: **`352 / 352 Passed (100%)`**
