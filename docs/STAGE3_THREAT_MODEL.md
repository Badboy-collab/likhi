# LIKHI — STAGE 3 PRIVACY THREAT MODEL
## STRIDE ANALYSIS & DE-ANONYMIZATION RISK MITIGATION

**Document Version:** 1.0.0  
**Status:** DRAFT SPECIFICATION (Threat Model Design Only — No Implementation)  
**Target:** Likhi (লিখি) Windows Typing System  

---

## 1. OBJECTIVE & SCOPE

This document performs a formal **STRIDE** (Spoofing, Tampering, Repudiation, Information Disclosure, Denial of Service, Elevation of Privilege) threat assessment and analyzes potential de-anonymization risks inherent in crowdsourced language modeling.

The primary security and privacy question is:
> *Can an adversary (observing network traffic, accessing database backups, or operating the server) reconstruct sensitive user documents, discover personal identities, or track individuals based on the learning data?*

---

## 2. STRIDE THREAT ASSESSMENT MATRIX

| Threat Category | Potential Vulnerability | Likelihood | Impact | Likhi Architectural Mitigation | Residual Risk |
| :--- | :--- | :---: | :---: | :--- | :---: |
| **S**poofing | Attacker impersonates the Likhi update server to push malicious updates. | Medium | Critical | All model binaries are cryptographically signed with **Ed25519**. The client verifies signatures locally before loading. | **Very Low** |
| **T**ampering | Attacker intercepts or tampers with `global_model.bin` in transit. | High | Critical | TLS 1.3 enforced. Client-side SHA-256 and Ed25519 signature checks reject modified files. | **Negligible** |
| **R**epudiation | Client denies sending an abusive submission batch. | Low | Low | System is intentionally anonymous by design. Abuse is mitigated at ingestion via rate-limiting and consensus ($k \ge 50$), not punitive client tracking. | **Acceptable** |
| **I**nformation Disclosure | Eavesdropper or server operator reconstructs private text from contributed signals. | High | Critical | Zero sentences, zero documents, zero keystrokes. Only decoupled single-word tuples with $K \ge 50$ threshold. No IP retention. | **Very Low** |
| **D**enial of Service | Botnet floods the contribution API to exhaust server resources. | High | Medium | Cloudflare / Envoy edge rate limiting, stateless Proof-of-Work (PoW) puzzle required per batch, 32 KB payload cap. | **Low** |
| **E**levation of Privilege | In-process TSF DLL exploited to gain system privilege in elevated host processes. | Medium | High | `bangla_tsf.dll` contains **no network code**, runs with standard host rights, and implements memory-safe boundary validation. | **Very Low** |

---

## 3. DE-ANONYMIZATION RISK & RE-IDENTIFICATION ANALYSIS

### Risk 1: Can a user be identified from rare or unique `roman_key` strings?
* **Threat Scenario:** A user types a unique personal identifier (e.g., their full name, credit card number, national ID, or unusual password like `johnsmith2026`). If uploaded, this could identify them.
* **Likhi Countermeasures:**
  1. **Strict Alpha-Only Regex:** `^[a-z]{1,24}$`. Any input containing digits (`0-9`), punctuation, uppercase letters, spaces, or symbols is strictly rejected and never staged.
  2. **Length Clamping:** Keys longer than 24 characters are discarded.
  3. **High-Frequency Gate ($freq \ge 3$):** A candidate is only eligible for contribution if selected multiple times across different days. Single accidental keystrokes, OTPs, or one-off sensitive tokens are never staged.
  4. **$k$-Anonymity Aggregation Threshold ($K \ge 50$):** Even if an uncommon name passes client filtering, the server will **never** output it into `global_model.bin` unless $\ge 50$ independent, cryptographically distinct clients submit the identical mapping.

---

### Risk 2: Can user typing habits be correlated via Timestamps or IP Addresses?
* **Threat Scenario:** An adversary analyzes upload timestamps or server access logs to correlate typing activity with specific individuals or timezones.
* **Likhi Countermeasures:**
  1. **Zero Millisecond Timestamps:** Submissions contain no time information more granular than the weekly calendar code (`2026-W37`).
  2. **Delayed Random Jitter:** The background worker does not upload immediately after typing. It flushes batches only during system idle, delayed by a randomized sleep window ($12 \text{ hours} \le \tau \le 36 \text{ hours}$).
  3. **Edge IP Stripping:** The reverse proxy decouples IP addresses at the TCP layer. IP addresses are never passed to the application database or persisted on disk.

---

### Risk 3: Can document context or complete sentences be reconstructed?
* **Threat Scenario:** If sequential word pairs are captured, an adversary could piece together complete sentences or confidential emails.
* **Likhi Countermeasures:**
  1. **Zero Sentence Context:** Likhi contributions consist strictly of isolated unigram pairs: `(roman_key, selected_bengali)`.
  2. **No N-Gram Sequence Linking:** Contributions are shuffled randomly in memory before serialization. There is no correlation or sequence index linking word $N$ with word $N+1$.
  3. **No Clipboard or Selection Reading:** Likhi does not inspect text already on the screen or clipboard.

---

## 4. IN-PROCESS HOST APPLICATION ISOLATION

`bangla_tsf.dll` loads directly into sensitive host applications:
* Windows Login / Credential Provider
* Microsoft Office (Word, Excel, PowerPoint)
* Web Browsers (Edge, Chrome, Firefox)
* System Consoles (PowerShell, CMD)

### Security Guarantees:
1. **Air-Gapped In-Process DLL:** `bangla_tsf.dll` has no dependency on `ws2_32.dll`, `wininet.dll`, or `urlmon.dll`. Even if an adversary exploited the engine, it has no socket capabilities to exfiltrate memory.
2. **Process Integrity Respect:** Likhi honors Windows UAC and AppContainer integrity levels. In protected sandbox processes (e.g. Chrome Renderers, Edge AppContainers), Likhi executes with strictly restricted privileges.
3. **Password Protection:** Under Windows TSF, fields marked with `TF_ES_PASSWORD` or `TS_SS_PASSWORD` cause Likhi to automatically bypass composition and pass all native keystrokes directly to the host without evaluation or learning.
