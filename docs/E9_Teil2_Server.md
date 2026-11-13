# E9, part 2 – Thread pool in the server and clean shutdown

This file explains the second part of E9, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. E9 status after part 2

| E9 DoD point | Where |
|---|---|
| server handles several requests **in parallel in a thread pool** | our `ThreadPool` plugged into cpp-httplib (`PoolWarteschlange`); test `BearbeitetAnfragenParallel` |
| results in a **cache shared by several threads** | `ErgebnisCache` in the server chain (`main.cpp`) |
| results **identical to sequential**, regardless of number and order | stress test (part 1) **and** 320 real HTTP requests from 8 parallel clients (`ParalleleClientsBekommenGleicheErgebnisseWieDirekt`) |
| identical requests **not computed more than once** | part 1 tests; over sockets: 320 requests, 8 distinct → 8 computations |
| **clean shutdown without hanging threads** | `stoppe()` → pool `beende()` → `join`; Ctrl+C/SIGTERM in `main`; tests with a 10 s limit |
| **stress test with fixed seed in bounded time** | part 1 (~0.25 s); all tests have a 60 s CTest timeout |

**E9 is complete.**

## 2. Files

| File | Change |
|---|---|
| `src/http_server.hpp` / `.cpp` | takes an `IAnfrageBehandler&` and a number of threads; `binde`/`lausche`/`warteBisBereit`/`stoppe`; adapter `PoolWarteschlange`; socket options |
| `src/main.cpp` | full chain from part 1, `--threads N`, signal thread for Ctrl+C / SIGTERM |
| `CMakeLists.txt` | `http_server.cpp` is now its own library `projekt_server`, used by the program **and** the tests |
| `tests/test_http_server.cpp` | 6 tests over real local sockets (`127.0.0.1`, free port, no internet) |

## 3. Our pool inside cpp-httplib

cpp-httplib accepts connections in `listen()`. Each connection becomes a task for a **`TaskQueue`**, an interface with
two functions: `enqueue(task)` and `shutdown()`. The library lets us provide our own queue:

```cpp
class PoolWarteschlange : public httplib::TaskQueue {     // adapter pattern
    bool enqueue(std::function<void()> aufgabe) override { return pool_.einreihenOhneErgebnis(std::move(aufgabe)); }
    void shutdown() override { pool_.beende(); }
    ThreadPool pool_;
};
server_->new_task_queue = [anzahl] { return new PoolWarteschlange(anzahl); };
```

That's the **adapter pattern**: our class speaks the library's interface and translates to our `ThreadPool`. The 1-thread
stopgap from E8 is gone. The number of threads comes from `--threads N` (default: number of CPU cores).

## 4. Clean shutdown, step by step

### 4.1 Inside `HttpServer`
1. `stoppe()` calls `server_->stop()`: the listening socket is closed, and the accept loop in `listen()` ends.
2. cpp-httplib then calls `task_queue->shutdown()` → our `ThreadPool::beende()`:
   - no new tasks,
   - **running requests are finished** (test `StopptSauberMitLaufenderAnfrage`: a 300 ms request started before the
     stop still gets its full answer),
   - every thread is `join`ed.
3. Only then does `lausche()` return.

`stoppe()` also calls `decommission()`. If the stop comes **before** `lausche()` has started (Ctrl+C right at startup),
`lausche()` doesn't start at all (test `StoppeVorLauschen`). Without that, a too-early Ctrl+C would be lost and the server
would run on.

**Keep-alive timeout 2 s:** browsers keep connections open for the next request. Such an idle connection occupies one pool
thread until it times out. The default is 5 s; 2 s makes shutdown faster.

### 4.2 Ctrl+C in `main.cpp`: a signal **thread**, not a signal **handler**
A classic signal handler interrupts *some* thread at *any* point. Inside it, almost no functions may be called (no
`mutex`, no `printf`, no memory allocation), so stopping a server from there isn't safe.

Instead:
1. **Before** any thread starts, `pthread_sigmask(SIG_BLOCK, {SIGINT, SIGTERM})` blocks both signals. New threads
   **inherit** this, so neither the pool threads nor `main` will be interrupted.
2. A dedicated `signalThread` calls `sigwait()`, which sleeps until one of the blocked signals arrives, and takes it
   **synchronously**. From then on it's normal code in a normal thread:
   `server.warteBisBereit(); server.stoppe();`
3. `main` returns from `lausche()`.
4. `main` then calls `stoppe()` once more and `pthread_kill(signalThread, SIGTERM)`. If `lausche()` ended **without** a
   signal (e.g. an error), the signal thread would otherwise wait forever and `join` would hang. Both calls are harmless
   if the signal already came.
5. `signalThread.join()`. Afterwards only `main` is left. "MarketLens beendet.", exit code 0.

### 4.3 Order of destruction
All objects in `main` are local variables. They are destroyed in **reverse order of creation**: server → result cache →
`Anfragebehandlung` → sources. Nothing is deleted while a thread could still use it, because the pool is already joined
when `lausche()` returns.

## 5. A bug found by a test: two servers on one port

The test `BelegterPort` expected a second server on an occupied port to fail. **It didn't.** cpp-httplib sets the socket
option **`SO_REUSEPORT`** by default, which lets several processes open the same port. The kernel then distributes
connections between them. A MarketLens started twice by mistake would silently split the requests instead of reporting
"port in use".

Fix: `set_socket_options` with **only `SO_REUSEADDR`**. That still lets the port be reused right after a restart (no
"address already in use" for about a minute), but a second **running** server is rejected. The test was right, and the code
was changed, not the test.

## 6. Tests (`tests/test_http_server.cpp`)

The helper `LaufenderServer` opens a free port on `127.0.0.1` (`binde(…, 0)`), runs `lausche()` in `std::async` and waits
until it's ready. `stoppeUndWarte()` returns `true` only if `lausche()` returns **within 10 s**, which is the check for
"no hanging threads".

| Test | Checks |
|---|---|
| `ParalleleClientsBekommenGleicheErgebnisseWieDirekt` | full chain as in `main`; 8 client threads × 5 rounds × 8 requests (incl. 404, 400) = 320 HTTP requests; each status and body equals the direct `Anfragebehandlung`; **8 computations** for 8 distinct requests; clean stop |
| `BearbeitetAnfragenParallel` | handler answers 200 only if **4 requests are inside at the same time** (otherwise 500 after 5 s) → all 4 get 200 |
| `StopptSauberMitLaufenderAnfrage` | stop during a 300 ms request: server ends within 10 s, the request still gets its full answer, new connections are refused |
| `StoppeVorLauschen` | `stoppe()` before `lausche()` → `lausche()` returns `false` at once |
| `BelegterPort` | second server on the same port → `-1` (see section 5) |
| `StatischeDateienUndJsonFuerAnderePfade` | `/` serves `index.html` from the web folder; unknown file → JSON 404; API still works |

These tests use **local** sockets only. They need no internet and no API key, so the test suite stays offline as `PROJEKT.md`
requires.

## 7. Manual check (2026-11-13)

```
./build/projekt --port 18081 --threads 4
```
| Check | Result |
|---|---|
| `GET /api/indikatoren?symbol=IBM` | 200 |
| a second `projekt` on the same port | "Fehler: Server konnte nicht auf 127.0.0.1:18081 starten" |
| threads of the running process (`/proc/<pid>/task`) | 6 = main + signal thread + 4 pool threads |
| `kill -INT` (= Ctrl+C) | "MarketLens beendet.", exit code 0, process gone |
| `kill -TERM` | same |

## 8. Result

169 of 169 tests pass (6 new). A full rebuild shows **0 warnings**. Server and stress tests pass in **10 repeated runs**
(`--gtest_repeat=10`). The ThreadSanitizer note from part 1 still applies (it can't run in the container; the commands are in
`E9_Teil1_Nebenlaeufigkeit.md`, section 8).

## 9. Possible exam questions

- *How does your own thread pool get into cpp-httplib?* → Through the library's `TaskQueue` interface; `PoolWarteschlange`
  adapts it to `ThreadPool` (adapter pattern).
- *What happens to a request that is running when the server is stopped?* → It finishes; `beende()` lets running and queued
  tasks complete before joining.
- *Why not stop the server from a signal handler?* → In a signal handler almost nothing is allowed (no mutex, no allocation).
  Blocking the signals and taking them with `sigwait` in a dedicated thread runs the shutdown as normal code.
- *Why must the signals be blocked before the other threads start?* → Threads inherit the signal mask; a thread created
  earlier could still be interrupted by the signal.
- *Why `pthread_kill` at the end of `main`?* → If the server ended without a signal, the signal thread would wait forever
  and `join` would hang.
- *What's the difference between `SO_REUSEADDR` and `SO_REUSEPORT`?* → `REUSEADDR` allows rebinding right after a restart;
  `REUSEPORT` lets several running processes share the port, which we don't want.
- *How do the tests show the server doesn't hang?* → `lausche()` runs in `std::async`; after `stoppe()` the future must be ready
  within 10 s; CTest also aborts any test after 60 s.
