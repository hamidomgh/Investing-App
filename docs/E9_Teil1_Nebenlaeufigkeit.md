# E9, part 1 – Thread pool, thread-safe result cache and stress test

This file explains the first part of E9, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. Goal (from `PROJEKT.md`) and what this part covers

> The server handles several requests in parallel in a thread pool. Computed results are kept in a cache that
> several threads access; results are identical to the sequential computation regardless of the number and order
> of requests, identical requests are not computed more than once, and the server shuts down cleanly without
> hanging threads. A stress test with a fixed seed runs in bounded time.

| DoD point | Part 1 (this file) | Part 2 |
|---|---|---|
| thread pool | **own `ThreadPool`**, tested | plugged into the server |
| cache shared by several threads | **`ErgebnisCache`**, tested | used by the server |
| results identical to sequential, independent of number/order | **stress test** with 1, 4, 16 threads and shuffled order | test over real sockets |
| identical requests computed only once | **tested** (sequential and truly simultaneous) | – |
| clean shutdown without hanging threads | **pool** shuts down cleanly (tested) | the **server** (Ctrl+C) |
| stress test with fixed seed, bounded time | **done**: seed 20261113, 2000 requests, ~0.25 s | – |

## 2. Files

| File | Purpose |
|---|---|
| `src/server/threadpool.hpp` / `.cpp` | `ThreadPool`: fixed number of worker threads and a task queue |
| `src/server/ergebnis_cache.hpp` / `.cpp` | `ErgebnisCache`: thread-safe, computes each key at most once at a time |
| `src/server/anfrage_behandler.hpp` | interface `IAnfrageBehandler` ("turns an `Anfrage` into an `Antwort`") |
| `src/server/gecachte_anfragebehandlung.hpp` / `.cpp` | decorator: asks the cache first, only then the real handler |
| `src/quellen/synchronisierte_quelle.hpp` | decorator: lets only one thread at a time into a price source |
| `src/server/anfragebehandlung.hpp` | **changed:** now implements `IAnfrageBehandler` (`override`), comment on thread safety |
| `tests/test_nebenlaeufigkeit.cpp` | 17 tests, including the stress test |
| `CMakeLists.txt` | new files; **every test now has a 60 s timeout** (a deadlock fails instead of hanging forever) |

## 3. How the pieces fit together (part 2 will build exactly this)

```
cpp-httplib ──► ThreadPool (N threads) ──► GecachteAnfragebehandlung ──► ErgebnisCache
                                                     │ (only on a cache miss)
                                                     ▼
                                              Anfragebehandlung  ──►  SynchronisierteQuelle ──► DateiQuelle / AlphaVantage + Cache (E7)
                                           (indicators computed           (one thread at a time
                                            in parallel)                   loads data)
```

## 4. `ThreadPool`

### 4.1 Idea
Starting a new thread for every request is expensive and unbounded. A pool starts **N threads once**. They take
tasks from a shared **queue** until the pool is shut down.

### 4.2 Shared state and how it's protected
| Member | Shared by | Protection |
|---|---|---|
| `aufgaben_` (queue) | the caller and all workers | `mutex_` |
| `beendet_` | the caller and all workers | `mutex_` |
| `signal_` (condition variable) | wakes sleeping workers | used together with `mutex_` |

The worker loop (`arbeite`):
```cpp
std::unique_lock<std::mutex> sperre(mutex_);
signal_.wait(sperre, [this] { return beendet_ || !aufgaben_.empty(); });  // sleep without busy-waiting
if (beendet_ && aufgaben_.empty()) return;                                // finished
aufgabe = take the front of the queue;
// the lock is released here (end of block)
aufgabe();                                                                // run WITHOUT the lock
```
- **`wait` with a condition:** the thread sleeps and uses no CPU. It re-checks the condition after waking up, which
  also handles *spurious wakeups* (waking up without a signal).
- **Run the task without the lock.** Otherwise only one thread could ever work, and the pool would be pointless.
- **Exceptions don't kill a worker.** An exception escaping a thread function would terminate the whole program, so
  the loop catches everything. Tasks from `einreihen` don't even get there: `packaged_task` stores their exception
  in the future.

### 4.3 Results: `einreihen` returns a `std::future`
```cpp
std::future<int> z = pool.einreihen([] { return 6 * 7; });
int x = z.get();   // waits for the result; rethrows the task's exception if it had one
```
Internally a `std::packaged_task` connects the task to the future. It's wrapped in a `shared_ptr`, because
`std::function` (the queue's element type) must be copyable and `packaged_task` is only movable. `einreihen` is a
**template in the header**, because its return type depends on the task (`int`, `Antwort` …).

### 4.4 Clean shutdown (`beende`, destructor)
1. Set `beendet_ = true` under the lock. From now on, new tasks are rejected (`einreihen` throws,
   `einreihenOhneErgebnis` returns `false`).
2. `notify_all()` wakes every sleeping worker.
3. Workers **finish the remaining queue**, then leave their loop.
4. `join()` waits for each thread. Afterwards **no pool thread is running**.

- The destructor calls `beende()`, so a pool going out of scope can never leave a thread behind (RAII).
- Calling `beende()` twice is harmless (`joinable()` check, plus its own mutex against two simultaneous callers).
- Calling `beende()` **from a pool thread** would mean a thread `join`ing itself, which is a deadlock. It throws
  `std::logic_error` instead.
- Copying is forbidden (`= delete`), because a pool owns threads.

## 5. `ErgebnisCache`: "identical requests are not computed more than once"

### 5.1 The problem
Two threads ask for `/api/indikatoren?symbol=IBM` **at the same moment**. A naive cache ("look up; if missing,
compute and store") lets both see "missing", so both compute. That's exactly what the DoD forbids.

### 5.2 The solution: store the *future* before computing
```
lock
  entry exists and (still running or fresh)?  → take its shared_future        (wait for it / get it)
  otherwise                                   → create promise, store its shared_future NOW, "I compute"
unlock
if I compute:  result = berechne()            ← runs WITHOUT the lock, other keys compute in parallel
               promise.set_value(result)      ← every waiting thread wakes up with the same result
               lock: mark entry finished (or drop it, see 5.3)
else:          return future.get()
```
- `std::shared_future` allows **many** threads to wait for and read the same result. (`std::future` allows only one `get()`.)
- The entry exists from the **start** of the computation, so the second thread finds it and waits instead of computing.
- The mutex is held only for the short map lookup, never during the computation.

### 5.3 What is kept, and for how long
| Rule | Why |
|---|---|
| **5xx answers are not kept** (`standardRegel`) | 502/503 are temporary (provider down, quota). Next time it should try again. Threads that were already waiting still get the answer |
| 4xx answers are kept | they only depend on the request (`sma=0` is always invalid) |
| an exception in `berechne` → passed to the caller **and** all waiters, entry removed | next request tries again |
| validity (e.g. 5 min) via `IUhr` | the data underneath can change. Same clock interface as E7, so it's testable with `FakeUhr`. Exactly at the limit counts as expired |
| `maxEintraege` (upper bound) | memory can't grow without limit. First expired entries are removed, then the oldest finished one. **Running entries are never removed**, because other threads are waiting on them |

### 5.4 Cache key (`GecachteAnfragebehandlung::schluessel`)
Method, path and all parameters, each written as `<length>:<text>;`.
- Parameters are in a `std::map`, which is always sorted, so `?sma=5&rsi=7` and `?rsi=7&sma=5` give the **same** key.
- The length prefix avoids collisions. Without it, the single value `a=b&c=` and the two parameters `a=b`, `c=` could
  produce the same text. The test checks this case.

## 6. `SynchronisierteQuelle`: why loading is serialized

`Anfragebehandlung` itself is thread-safe: `behandle` only **reads** its members (catalog, rate decisions). But below
it there is state that isn't made for several threads. The disk cache (E7) writes files (two threads writing the same
`.tmp` file would corrupt it), and later there's an HTTPS connection.

`SynchronisierteQuelle` is a **decorator** around any `IPriceSource` with one mutex. Only one thread at a time loads
data, while **computing** the indicators still runs in parallel.

**Trade-off:** loading different symbols also waits in line. With the provider's free quota (a few requests per
minute) that costs practically nothing, and it's far simpler and safer than making every class below thread-safe on its
own. It's documented in the header.

`std::lock_guard` releases the mutex automatically at the end of the function, **even when an exception is thrown**
(RAII). That matters, because sources throw `QuellenFehler`.

## 7. Tests (`tests/test_nebenlaeufigkeit.cpp`)

| Test | Checks |
|---|---|
| `ThreadPool.ErgebnisseUeberFutures` | 100 tasks, sum of squares 0..99 = 328350 |
| `ThreadPool.ArbeitetParallel` | 4 tasks wait until all 4 run **at the same time**. Only possible if the pool really is parallel (timeout 5 s instead of hanging) |
| `ThreadPool.ExceptionKommtImFutureAn` | exception reaches `get()`; the pool keeps working |
| `ThreadPool.BeendeArbeitetWarteschlangeAb` | 1 thread, 50 queued tasks, `beende()` → all 50 done |
| `ThreadPool.NachBeendenKeineNeuenAufgaben` | second `beende()` harmless; new tasks rejected |
| `ThreadPool.DestruktorBeendetSauber` | pool leaves scope with work queued → all 20 tasks done, no hang |
| `ThreadPool.UngueltigeBenutzung` | 0 threads; `beende()` from a pool thread → `logic_error` |
| `ErgebnisCache.GleicherSchluesselNurEinmal` | same key → 1 computation, other key → new computation |
| `ErgebnisCache.GleichzeitigeAnfragenNurEinmalBerechnet` | 1 thread computes and is **held**, 7 more ask for the same key meanwhile → **1 computation**, all 8 get the result |
| `ErgebnisCache.AbgelaufenWirdNeuBerechnet` | 4:59 fresh, 5:00 expired |
| `ErgebnisCache.ServerfehlerWerdenNichtGespeichert` | 502 computed twice, 400 once |
| `ErgebnisCache.ExceptionWirdNichtGespeichert` | exception passed on, no entry left, next call works |
| `ErgebnisCache.ObergrenzeWirdEingehalten` | limit 2: oldest is evicted |
| `ErgebnisCache.UngueltigeKonfiguration` | negative validity, limit 0 |
| `GecachteAnfragebehandlung.Schluessel` | order independent; different value/path/method differ; no collision with `&`/`=` |
| `SynchronisierteQuelle.NieZweiGleichzeitig` | 8 threads × 25 loads; the inner source detects any overlap → never two at once |
| `Stresstest.ParallelGleichSequentiellUndOhneDoppelteBerechnung` | see below |

### The stress test
1. **Fixed seed** (`std::mt19937` with 20261113) generates **2000 random requests**: all 4 endpoints and an unknown path,
   3 known and 1 unknown symbol, valid and invalid periods (`sma=0`), invalid dates (month 13), MACD with fast ≥ slow, and
   too long periods (`sma=100`). Same seed, same requests on every run, so a failure can be reproduced.
2. **Reference:** each distinct request is computed **sequentially** with a plain `Anfragebehandlung`, without threads and
   without a cache. The test also checks that the mix really contains 200, 400, 404 and 422.
3. **Parallel:** three runs with **1, 4 and 16 threads**. Each run uses a **different shuffled order** (also seeded) through
   `ThreadPool` → `GecachteAnfragebehandlung` → counting decorator → `Anfragebehandlung` → `SynchronisierteQuelle`.
4. For **every one of the 2000 requests**, status and JSON body must be **byte-identical** to the reference.
5. The real handler must have run **exactly once per distinct request** (counted with `std::atomic`).
6. **Bounded time:** all three runs together must take under 30 s. They take about 0.25 s. CTest also aborts any
   test after 60 s.

The test source (`TestQuelle`) is deliberately **not** thread-safe: it uses a plain `int` counter. It's only safe because it
sits behind `SynchronisierteQuelle`.

## 8. Result and an honest limitation

- 163 of 163 tests pass (17 new). A full rebuild shows 0 warnings.
- The concurrency tests passed in **5 repeated runs** (`--gtest_repeat=5`).
- **ThreadSanitizer could not be run in the container.** TSan detects data races while the program runs, which is stronger
  evidence than passing tests. It fails at startup with `unexpected memory mapping`, a known clash with the address-space
  randomization of newer kernels. The usual workaround (`setarch -R`) is blocked by the container's security settings. To run
  it, start the container without the seccomp filter (only for this check):
  ```bash
  FAUP_RUN_ARGS="--security-opt seccomp=unconfined" ./dev.sh
  cmake -B build-tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
  cmake --build build-tsan
  setarch "$(uname -m)" -R ./build-tsan/tests --gtest_filter='Stresstest.*:ThreadPool.*:ErgebnisCache.*:SynchronisierteQuelle.*'
  ```
  No `WARNING: ThreadSanitizer` in the output means no race was detected.

## 9. Next: part 2

- `HttpServer` uses our `ThreadPool` through a small adapter to cpp-httplib's `TaskQueue` interface (`enqueue`, `shutdown`),
  instead of the single thread from E8.
- `main.cpp` builds the chain from section 3.
- Clean shutdown on Ctrl+C / SIGTERM: a separate thread waits for the signal (`sigwait`) and calls `stoppe()`. Then the pool
  is shut down and all threads are joined.
- A test over real local sockets (`127.0.0.1`, free port): parallel HTTP requests, then stop, and the server thread must end
  within a time limit.

## 10. Possible exam questions

- *What's shared between the threads in the pool, and how is it protected?* → The queue and the shutdown flag, by one mutex;
  a condition variable lets workers sleep until there's work.
- *Why run the task without holding the lock?* → Otherwise only one task could run at a time.
- *How do you prevent two threads from computing the same request?* → Store a `shared_future` in the map **before** computing;
  the second thread finds it and waits.
- *Why `shared_future` and not `future`?* → Several threads must be able to read the same result.
- *Why aren't 5xx answers cached?* → They're temporary; the next request should try the provider again.
- *Why can't a running entry be evicted?* → Other threads are waiting for its result.
- *How does the stress test show "identical to sequential"?* → Same 2000 requests, sequential reference vs. 1/4/16 threads in
  shuffled order; every status and body must match byte for byte.
- *Why a fixed seed?* → Reproducible: a failing run can be repeated exactly.
- *Why is loading serialized?* → The layers below (disk cache, later HTTPS) aren't thread-safe; one mutex in a decorator is
  simple and enough for the provider's request limits.
- *What does RAII mean here?* → `lock_guard` unlocks and `~ThreadPool` joins automatically, also when an exception occurs.
