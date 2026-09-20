# LIKHI — STAGE 3 API SPECIFICATION
## MINIMAL, ANONYMOUS, OFFLINE-FALLBACK REST API SPECIFICATION

**Document Version:** 1.0.0  
**Status:** DRAFT SPECIFICATION (API Design Only — No Implementation)  
**Target:** Likhi Global Learning Server (`api.likhi.org`)  

---

## 1. DESIGN GOALS & CONSTRAINTS

1. **Strictly Secondary:** The Likhi client application never requires the API for normal, high-speed typing. Typing latency is 0.00 ms dependent on the network.
2. **Anonymous & Stateless:** The API does not use user accounts, session cookies, tracking tokens, or persistent bearer tokens.
3. **Bandwidth Minimization:** Delta models, HTTP caching (`ETag`, `If-None-Match`), and gzip/brotli compression ensure total monthly network usage is $< 5$ MB per active machine.
4. **Resilience to Failure:** Any network failure, DNS error, timeout, or 5xx server response silently falls back to local offline operation. Zero user-visible error dialogues.

---

## 2. ENDPOINTS

### 2.1 GET `/api/v1/pow/challenge`
Obtains a short-lived, stateless cryptographic puzzle required before submitting a learning batch.

* **Method:** `GET`
* **Headers:**
  - `User-Agent: Likhi-Sync/1.0.0 (Windows NT 10.0; x64)`
* **Response (200 OK):**
  ```json
  {
    "challenge_id": "c7a8b9e0-1234-5678-9abc-def012345678",
    "algorithm": "sha256",
    "prefix": "likhi-pow-20260909-",
    "difficulty_zeros": 18,
    "expires_at": 1788953400
  }
  ```

---

### 2.2 POST `/api/v1/contribute`
Submits an anonymous batch of crowdsourced suggestion selections.

* **Method:** `POST`
* **Rate Limit:** 1 request per 24 hours per client; max 10 requests per minute per IP address.
* **Headers:**
  - `Content-Type: application/json; charset=utf-8`
  - `X-Likhi-Client-Version: 1.0.0`
  - `X-Likhi-PoW-Challenge: c7a8b9e0-1234-5678-9abc-def012345678`
  - `X-Likhi-PoW-Nonce: 8493120`
* **Request Body:**
  ```json
  {
    "schema_version": "1.0",
    "contributions": [
      {
        "roman_key": "porishkar",
        "selected_candidate": "পরিষ্কার",
        "vote_weight": 1
      },
      {
        "roman_key": "sundor",
        "selected_candidate": "সুন্দর",
        "vote_weight": 1
      }
    ]
  }
  ```
* **Responses:**
  - `204 No Content`: Successfully accepted and enqueued into the aggregation buffer.
  - `400 Bad Request`: Schema validation error or invalid Unicode characters.
  - `403 Forbidden`: PoW challenge invalid or expired.
  - `429 Too Many Requests`: Rate limit exceeded. (Client retries after 24 hours).
  - `503 Service Unavailable`: Server undergoing maintenance.

---

### 2.3 GET `/api/v1/model/version`
Checks for the latest compiled Global Model release.

* **Method:** `GET`
* **Headers:**
  - `If-None-Match: "w/10001"` (Client's current model ETag)
* **Responses:**
  - `304 Not Modified`: Local model is already up-to-date.
  - `200 OK`: New model available.
    ```json
    {
      "latest_model_version": "1.0.1",
      "version_code": 10001,
      "release_timestamp": 1788950000,
      "min_client_version": "1.0.0",
      "download_url": "https://cdn.likhi.org/models/global_model_v1.0.1.bin",
      "file_size_bytes": 1048576,
      "sha256": "5A3D8E...7F1B",
      "ed25519_signature": "C4B910...D98E"
    }
    ```

---

### 2.4 GET `/models/global_model_v{version}.bin`
Downloads the compiled binary Global Model file.

* **Method:** `GET`
* **Headers:**
  - `Range: bytes=0-` (Supports standard chunked and resumable downloads)
* **Response (200 OK / 206 Partial Content):**
  - Content-Type: `application/octet-stream`
  - Binary stream of `global_model.bin` conforming to the LGM1 format specification.

---

## 3. CLIENT RETRY & ERROR HANDLING SPECIFICATION

The background sync worker (`likhi_sync.exe`) strictly implements **Exponential Backoff with Full Jitter** to protect both server infrastructure and client battery/CPU resources.

### Backoff Algorithm:
```
BaseDelay = 60 seconds
MaxDelay  = 86400 seconds (24 hours)

Attempt = 0, 1, 2, ...
Delay = min(MaxDelay, BaseDelay * (2 ^ Attempt))
ActualSleep = UniformRandom(Delay / 2, Delay)
```

### Response Action Matrix:

| Error Type | Client Action | Next Retry Interval |
| :--- | :--- | :--- |
| **No Network / DNS Resolution Failure** | Silently abort. Maintain local staging queue intact. | Next system network-change notification or 4 hours. |
| **Connection Timeout ($> 5$s)** | Silently abort connection. | 2 hours. |
| **HTTP 429 (Too Many Requests)** | Respect `Retry-After` header if present. | Default 24 hours. |
| **HTTP 500 / 502 / 503 / 504** | Log to local debug log (if logging enabled). | Exponential backoff (1h $\to$ 2h $\to$ 4h). |
| **SHA-256 / Ed25519 Verification Failure** | Discard downloaded file immediately. | 24 hours (do not retry bad payload). |

---

## 4. LOCAL-FIRST OFFLINE GUARANTEE

```
+-----------------------------------------------------------------------------+
|                          IS INTERNET AVAILABLE?                             |
|                                                                             |
|            YES                                          NO                  |
|             |                                            |                  |
|             v                                            v                  |
|   Check for updates in                      Continue normal typing.         |
|   background. No UI latency.                 Zero alerts. Zero popups.      |
|   Download verified model.                   100% features work offline.    |
+-----------------------------------------------------------------------------+
```

* **No Blocking:** The core typing engine never awaits network responses.
* **No Telemetry Pings:** If a user never enables global learning, zero bytes of network traffic will ever be generated to `api.likhi.org`.
