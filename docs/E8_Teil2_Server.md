# E8, part 2 – The real HTTP server and the program

This file explains the second part of E8, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. What this part adds

| E8 DoD point | Status after part 2 |
|---|---|
| server offers the 4 endpoints | **done**: a real HTTP server, checked with `curl` (section 7) |
| parameters checked, 404/400 with JSON | done in part 1; now also for paths outside `/api` |
| logic testable without a socket | done in part 1; the network layer has no logic of its own |

**E8 is now complete.** Not part of E8 and still open: live HTTPS access (needs `libssl-dev`, decided "not yet")
and rate decisions for the event study (FRED source, E6).

## 2. Files

| File | Purpose |
|---|---|
| `third_party/httplib.h` | cpp-httplib 0.18.3, header-only, MIT, unchanged |
| `third_party/README.md` | **changed:** version, source and SHA-256 of `httplib.h` |
| `src/http_server.hpp` / `.cpp` | class `HttpServer`: the thin network layer |
| `src/dateiquelle.hpp` / `.cpp` | class `DateiQuelle`: offline `IPriceSource` reading `daten/<SYMBOL>.json` |
| `src/main.cpp` | **replaced:** starts the MarketLens server (was the `Statistik` demo of the template) |
| `daten/IBM.json`, `daten/README.md` | offline data: your recorded IBM answer, and how to add more |
| `tests/test_dateiquelle.cpp` | 4 tests |
| `CMakeLists.txt` | `projekt` = `main.cpp` + `http_server.cpp`, include path for `httplib.h`, Threads |

The `Statistik` class and its tests from the template are unchanged and still build. Only `main.cpp` no longer uses it.

## 3. `HttpServer`: translation only

```
HTTP request ──► httplib::Request ──► Anfrage ──► Anfragebehandlung::behandle ──► Antwort ──► httplib::Response
```

The constructor sets up everything:
1. **One worker thread** (`new_task_queue = … ThreadPool(1)`). cpp-httplib normally handles requests in several
   threads at once. But `Anfragebehandlung`, `DateiQuelle` and the cache (E7) aren't thread-safe yet. That's
   exactly the job of E9. Until then requests are handled **one after another**, which is safe.
2. **Static files** from `web/` (E10), if the folder exists. For `/` cpp-httplib serves `index.html`.
3. **One route for everything**: `Get(".*")`, plus `Post`, `Put`, `Delete`, `Patch`. cpp-httplib first tries the
   static files, and everything else goes to `Anfragebehandlung`. So `/foo` also gets a **JSON** 404, and POST
   gets a JSON 405. No special cases in the network layer.
4. The lambda `weiterleiten` copies method, path and parameters into an `Anfrage`. If a parameter appears twice
   (`?sma=5&sma=6`), the first one counts (`emplace` doesn't overwrite).

**Why `http_server.cpp` is not in `projekt_lib`:** it contains no logic, only translation. `projekt_lib` holds
everything that's tested. Keeping `httplib.h` out of the library also keeps the test build fast.

**Why a forward declaration (`namespace httplib { class Server; }`) in the header:** `http_server.hpp` stores
only a `std::unique_ptr<httplib::Server>`, so it doesn't need the full class. Only `http_server.cpp` includes the
340 KB `httplib.h`. The destructor must then be defined in the `.cpp` (`~HttpServer() = default;` there), because
`unique_ptr` can only delete a complete type.

## 4. `DateiQuelle`: offline prices

The live HTTPS client doesn't exist yet. Without it nothing would ever reach the cache, and the server couldn't
deliver any prices. `DateiQuelle` is a small `IPriceSource` that reads `daten/<SYMBOL>.json` and parses it with
**the same** `AlphaVantageQuelle::parse`. So offline files are checked exactly like real answers.

- Missing file → `QuellenFehler` with the path → the API answers **502** for this security, and the others keep working.
- **Symbol check:** only letters, digits and `. - _ ^`, and not `.` or `..`. A symbol like `../geheim` can't read
  files outside the folder (**path traversal**). Symbols come from the catalog, not from the user (unknown symbols
  are rejected with 404 first), so this is defence in depth.
- When the live client comes, `main.cpp` only swaps `DateiQuelle` for `AlphaVantageQuelle` + `CachingHttpClient`.
  Thanks to `IPriceSource` nothing else changes.

## 5. `main.cpp`

```
projekt [--host 127.0.0.1] [--port 8080] [--katalog config/katalog.json] [--daten daten] [--web web] [--hilfe]
```
1. Read the arguments (port checked: whole number 1–65535).
2. Read the catalog. Faulty entries are printed as warnings and skipped; an unusable catalog stops the program with exit code 1.
3. `DateiQuelle` → `Anfragebehandlung` (no rate decisions yet) → `HttpServer` → `starte()` blocks.
4. Port in use → message and exit code 1.

**Host `127.0.0.1` by default:** only reachable from the same machine (or container). That's the safe default;
binding to `0.0.0.0` opens the port to the whole network.

**Stopping:** Ctrl+C ends the process. A clean shutdown without hanging threads is a DoD point of **E9**, together
with the thread pool, so I'm not adding signal handling now.

## 6. How to run it

Inside the container, from the repository folder:
```bash
cmake -B build && cmake --build build
./build/projekt
# in a second shell in the container:
curl "http://127.0.0.1:8080/api/indikatoren?symbol=IBM"
```

To open it **from WSL or the browser**, the container needs a published port. `dev.sh` passes `FAUP_RUN_ARGS`
to podman:
```bash
FAUP_RUN_ARGS="-p 8080:8080" ./dev.sh          # start the container with the port published
./build/projekt --host 0.0.0.0                 # inside the container
# then in the browser on Windows/WSL: http://localhost:8080/api/wertpapiere
```
`--host 0.0.0.0` is needed because traffic from the published port arrives on the container's network interface,
not on its `127.0.0.1`.

## 7. Manual test (done on 2026-11-13)

Server started with `./build/projekt --port 18080`, requests with `curl`:

| Request | Result |
|---|---|
| `GET /api/wertpapiere` | 200, 6 securities |
| `GET /api/kurse?symbol=IBM&von=2026-11-10` | 200, candles from 2026-11-10, `veraltet: false` |
| `GET /api/indikatoren?symbol=IBM` | 200, every series with 100 entries, 8 levels, pivot points, volatility 0.56, max drawdown 0.33 |
| `GET /api/ereignisstudie?symbol=IBM` | 200, `ereignisse: []` (no rate decisions loaded yet) |
| `GET /api/kurse?symbol=MBG.DEX` | 502, "No offline data for MBG.DEX: file not found: daten/MBG.DEX.json" |
| `GET /api/kurse?symbol=MSFT` | 404, "unknown symbol 'MSFT'" |
| `GET /api/indikatoren?symbol=IBM&sma=0` | 400, "parameter 'sma' must be a positive integer, got '0'" |
| `GET /foo` | 404 JSON, "unknown path '/foo'" |
| `POST /api/kurse` | 405 JSON |
| content type | `application/json; charset=utf-8` |

This is the first piece of the **manual test protocol** that `PROJEKT.md` mentions for live access and the web UI.

## 8. Tests (`tests/test_dateiquelle.cpp`)

| Test | Checks |
|---|---|
| `LiestGespeicherteAntwort` | `daily_ok.json` → 3 candles |
| `FehlendeDatei` | `QuellenFehler` naming `GIBTSNICHT.json` |
| `InhaltWirdGeprueft` | `"Error Message"` → `HttpFehler`, broken JSON → `ParseFehler` (same as live) |
| `UnzulaessigeSymbole` | `../geheim`, `a/b`, `..`, `a b` rejected; `MBG.DEX`, `^GDAXI`, `BRK-B` allowed |

`HttpServer` has no automated test on purpose: it would need a socket, and the DoD only requires the **logic**
to be testable without one. Its behaviour is covered by the manual test above.

## 9. Result

146 of 146 tests pass (4 new), full rebuild with **0 warnings**.

## 10. Possible exam questions

- *Where is the logic of the server?* → In `Anfragebehandlung` (tested without socket). `HttpServer` only translates.
- *Why one worker thread?* → The handler and the cache aren't thread-safe yet; E9 adds thread safety and a real pool.
- *Why does `/foo` return JSON and not the library's default page?* → One catch-all route sends every non-file
  request to `Anfragebehandlung`, which answers unknown paths with a JSON 404.
- *Why a forward declaration and `unique_ptr`?* → Keeps `httplib.h` out of the header; the destructor is defined in
  the `.cpp` where the type is complete.
- *Why `127.0.0.1` by default?* → Safe default; `0.0.0.0` exposes the server to the network.
- *What is path traversal and how does `DateiQuelle` prevent it?* → Using `../` in a name to read files outside a
  folder. Only safe characters are allowed in symbols.
- *How do you switch to live data later?* → Replace `DateiQuelle` with `AlphaVantageQuelle` over `CachingHttpClient`
  in `main.cpp`; the rest only knows `IPriceSource`.
