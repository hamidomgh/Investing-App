# F5 (Part 1) – Live HTTP(S) client: what was built and why

This file explains part 1 of feature F5 so you can explain it in the exam. The specification (German) is
`Features/F5_Live_Zugriff.md`. The code comments are in German; this text is in English.
Built on the Git branch **`live-zugriff`**.

## 1. What it does

Until now nothing in MarketLens talked to the internet: both servers read recorded files from `daten/<provider>/`.
Part 1 adds the missing last link of the chain, a real network client:

```
AlphaVantageQuelle / TwelveDataQuelle  →  CachingHttpClient (E7)  →  LiveHttpClient  →  internet
          (already existed)                  (already existed)          (NEW)
```

`LiveHttpClient` implements the existing interface `IHttpClient` (one method: `get(url)`). Because the sources only know
the interface, **no source and no analysis code changes**. Part 2 will plug it into `main`.

## 2. Files

| File | Change |
|---|---|
| `src/http/live_http_client.hpp` / `.cpp` | **new**: `LiveHttpClient : IHttpClient` |
| `tests/test_live_http_client.cpp` | **new**: 8 tests against a local test server |
| `CMakeLists.txt` | client added to `projekt_server`; new option `MARKETLENS_LIVE_HTTP` (default OFF) |
| `container/Containerfile` | `libssl-dev` (OpenSSL headers) added |

## 3. How `get(url)` works

1. **Split the URL** (`zerlege`): `https://api.twelvedata.com/time_series?symbol=IBM&apikey=…` becomes
   base `https://api.twelvedata.com` and path `/time_series?symbol=IBM&apikey=…`. `httplib::Client` wants exactly that.
   Anything that doesn't start with `http://` or `https://` is a `KonfigurationsFehler`.
2. **HTTPS without OpenSSL?** Then throw a `KonfigurationsFehler` that names the CMake option, instead of a cryptic
   httplib error.
3. **A new `httplib::Client` per request** with a connect timeout (10 s) and read timeout (30 s), and certificate
   verification for HTTPS.
4. **Answer received** → return status and body **unchanged, even 401/404/429/500**. That is the `IHttpClient` contract:
   `pruefeStatus()` and the cache's rating functions decide what an error status means, not the client.
5. **No answer** → exception from the existing hierarchy in `quellenfehler.hpp`:
   - connect took too long → `ZeitueberschreitungFehler`
   - read error after (almost) the whole read timeout → `ZeitueberschreitungFehler`
   - anything else (refused, DNS, TLS, certificate) → `NetzwerkFehler`

   Because `ZeitueberschreitungFehler` derives from `NetzwerkFehler`, the cache (E7) catches both with one `catch` and
   falls back to stale data.

## 4. Design decisions

| Decision | Why |
|---|---|
| Client also supports plain `http://` | The tests can talk to a **local** `httplib::Server` on `127.0.0.1`: real sockets, real HTTP, but no internet, no API key, no quota. That is why the tests also run in the normal build. |
| OpenSSL only with `MARKETLENS_LIVE_HTTP=ON` | The default build stays network-free and needs no new package (PLAN.md item 14). |
| `CPPHTTPLIB_OPENSSL_SUPPORT` is `PUBLIC` on `projekt_server` | `httplib.h` is a header-only library. Every file that includes it must see the **same** setting, otherwise classes would have different layouts in different files (ODR violation). `PUBLIC` passes the macro on to `projekt` and `tests`. |
| Client in `projekt_server`, not `projekt_lib` | Only that library compiles the big `httplib.h`; `projekt_lib` stays free of it. |
| New `httplib::Client` per request | No shared state → safe from several threads, nothing to lock. One connection per request costs a little time, but there are only a few requests per day. |
| Error messages show only scheme, host and port | The URL contains the **API key** (`apikey=…`). It must never appear in a log or in the browser. A test checks this. |
| Timeout detection by measuring the time | httplib 0.18.3 reports a read timeout only as `Error::Read`. If the read failed after at least 90 % of the read timeout, it was a timeout; an early read error is a normal network error. |

## 5. Tests (8 new, `tests/test_live_http_client.cpp`)

| Test | Checks |
|---|---|
| `LiveHttpClientUrl.Zerlegung` | base/path split, with port, without path, query without path |
| `LiveHttpClientUrl.UngueltigeUrl` | `ftp://`, no scheme, no host → `KonfigurationsFehler` |
| `LiveHttpClient.GueltigeAntwort` | 200 and body arrive; the query reaches the server |
| `LiveHttpClient.FehlerstatusWirdZurueckgegeben` | 401/404/429/500 are returned, not thrown |
| `LiveHttpClient.KeineVerbindungIstNetzwerkFehler` | refused connection → `NetzwerkFehler` (not timeout); message has host, not the key |
| `LiveHttpClient.LangsamerServerIstZeitueberschreitung` | server needs 1 s, client allows 200 ms → `ZeitueberschreitungFehler` |
| `LiveHttpClient.HttpsOhneOpenSslIstKonfigurationsFehler` | clear message naming the option, no key in it (skipped in the live build) |
| `LiveHttpClient.MitCacheBeiNetzausfallAlteDaten` | whole chain: first request live and cached; server gone + 2 h later → stale data with a note |

**Results:** 217 of 217 tests pass (209 before + 8 new), no warnings; the 8 new ones also pass 10 times in a row.
With `-DMARKETLENS_LIVE_HTTP=ON` CMake currently stops with "Could NOT find OpenSSL", as expected until the image is rebuilt.

**A bug found while writing the tests:** the first helper for "a port nobody listens on" only called
`bind_to_any_port()`. httplib already calls `listen()` there, and without a running server `stop()` does not close the
socket. So the kernel accepted the connection and nobody answered: a timeout instead of "refused". The fix starts a real
test server, remembers its port and stops it properly.

## 6. To do before part 2 (you, in WSL)

```
./dev.sh build                      # rebuild the image with libssl-dev
# then restart the container (./dev.sh)
```
Then a live build in its **own** folder, so the normal `build/` stays offline (build without bare `-j`, 3.7 GiB RAM):
```
cmake -B build-live -DMARKETLENS_LIVE_HTTP=ON && cmake --build build-live && ctest --test-dir build-live
```

## 7. Possible exam questions

- *Why does nothing in the sources change?* → They only know `IHttpClient`; the live client is a new implementation (dependency inversion), like `FakeHttpClient` in the tests.
- *How do you test a network client without the internet?* → A local `httplib::Server` on `127.0.0.1` with a free port; the client speaks plain HTTP to it.
- *Why return a 429 instead of throwing?* → Contract of `IHttpClient`: the provider-specific rating (`bewerte`) and `pruefeStatus` decide; Alpha Vantage even reports errors with HTTP 200.
- *What happens if the internet is down?* → `NetzwerkFehler`; the `CachingHttpClient` catches it and serves the old data with a note.
- *Why is the API key safe?* → Only scheme/host/port go into error messages; a test checks that the key is not in the message.
- *Why the `PUBLIC` compile definition?* → Header-only library: all files must see the same configuration (ODR).
