# E2, part 1 – Interfaces, errors, API key and fake client

This file explains the first part of E2, so you can explain it in the exam.
The code comments are in German (like the rest of the project); this text is in English.

## 1. What E2 needs, and what this part covers

E2 (from `PROJEKT.md`): price data is loaded through `IPriceSource`, network access goes through
`IHttpClient`, JSON is turned into validated candles, error cases are distinguished, a missing API key
is reported before the first request, and a catalog describes the securities.

| E2 requirement | Status |
|---|---|
| `IHttpClient` interface | **done** (this part) |
| `IPriceSource` interface | **done** (this part) |
| Distinguish network error, timeout, HTTP status (401, 404), quota (429) | **done** (this part) |
| Missing API key reported before the first request | **done** for reading the key; the "before the first request" check comes with the Alpha Vantage source |
| Invalid JSON, missing fields (`ParseFehler`) | error class done; parsing comes in part 2 (needs nlohmann/json) |
| Alpha Vantage implementation of `IPriceSource` | part 2 |
| Catalog file, a faulty entry doesn't stop the others | part 3 |
| Real HTTPS client behind a CMake option | later (cpp-httplib + OpenSSL) |

This part needs **no external library**.

## 2. Files

| File | Purpose |
|---|---|
| `src/quellen/quellenfehler.hpp` | Error class hierarchy for all data sources |
| `src/http/http_client.hpp` / `.cpp` | `HttpAntwort`, interface `IHttpClient`, function `pruefeStatus` |
| `src/quellen/price_source.hpp` | Interface `IPriceSource` |
| `src/basis/umgebung.hpp` / `.cpp` | `leseApiSchluessel`: reads the API key from an environment variable |
| `tests/fake_http_client.hpp` | `FakeHttpClient`: test-only `IHttpClient` without a network |
| `tests/test_quellen.cpp` | 14 tests |
| `CMakeLists.txt` | the two new `.cpp` files and the test file are registered |

## 3. The error hierarchy (`quellenfehler.hpp`)

```
std::runtime_error
└── QuellenFehler                     any problem with a data source
    ├── NetzwerkFehler                no connection (network down, DNS, TLS)
    │   └── ZeitueberschreitungFehler server didn't answer in time
    ├── HttpFehler(status)            server answered with an error status
    ├── KontingentFehler              request quota exhausted (429)
    ├── ParseFehler                   invalid JSON or missing field
    └── KonfigurationsFehler          e.g. API key missing
```

**Why a hierarchy (inheritance) and not one class with an error code?**
A `catch` block can choose how specific it wants to be:
- `catch (const QuellenFehler&)`: "something went wrong with the source". Used for the catalog,
  where one faulty security mustn't stop the others.
- `catch (const NetzwerkFehler&)` or `catch (const KontingentFehler&)`: used by E7, which then
  delivers old cached data instead.
- `EXPECT_THROW(..., KontingentFehler)` in tests checks exactly one case.

**Design details:**
- `ZeitueberschreitungFehler` inherits from `NetzwerkFehler`, because a timeout *is* a network
  problem. Code that handles network errors handles timeouts automatically.
- `KontingentFehler` is **not** an `HttpFehler`. Alpha Vantage reports an exhausted quota with
  HTTP **200** and a note in the body, so a quota error doesn't always have an HTTP status.
- `HttpFehler` stores the status (`status()`), so callers can tell 401 and 404 apart.
- `using std::runtime_error::runtime_error;` takes over the base-class constructor. That's why
  `NetzwerkFehler("message")` works without writing a constructor.
- Everything derives from `std::runtime_error` (not `std::invalid_argument` like E1–E6): these are
  problems of the outside world at runtime, not invalid arguments from the programmer.

## 4. `IHttpClient` and `HttpAntwort` (`http_client.hpp`)

```cpp
struct HttpAntwort { int status; std::string body; };

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpAntwort get(const std::string& url) = 0;
};
```

- **Interface** = a class with only pure virtual functions (`= 0`). It says *what* can be done, not
  *how*. There will be two implementations: `FakeHttpClient` for tests and a real HTTPS client for
  live mode.
- **Virtual destructor:** if an object is deleted through an `IHttpClient*`, the destructor of the
  real class must run. Without `virtual` that's undefined behaviour.
- **Contract** (written in the header):
  - If the server answers, `get` **returns** the answer, even for 404 or 500.
  - If no answer comes, `get` **throws** `NetzwerkFehler` or `ZeitueberschreitungFehler`.

  This keeps every implementation small. How a status is interpreted is decided in **one** place:

### `pruefeStatus(antwort, kontext)`

| Status | Result |
|---|---|
| 200–299 | OK, nothing happens |
| 401, 403 | `HttpFehler`: "API key invalid or not permitted" |
| 404 | `HttpFehler`: "not found, check the symbol" |
| 429 | `KontingentFehler`: "request quota exhausted" |
| anything else | `HttpFehler` with the code |

`kontext` (e.g. "prices for AAPL") goes into the message, so the user knows which request failed.
The same function will be used by the rate source in E6.

## 5. `IPriceSource` (`price_source.hpp`)

```cpp
class IPriceSource {
public:
    virtual ~IPriceSource() = default;
    virtual Kursreihe ladeKurse(const Wertpapier& wertpapier) = 0;
};
```

The analysis (E3–E6) and later the server (E8) only know this interface. Changing the provider
(Alpha Vantage → Twelve Data) means writing a new class, without touching the analysis code. That
is the **dependency inversion** idea: high-level code depends on an abstraction, not on a concrete
provider.

## 6. API key (`umgebung.hpp` / `.cpp`)

`leseApiSchluessel("ALPHAVANTAGE_API_KEY")` reads the variable with `std::getenv`.
- Missing **or empty** → `KonfigurationsFehler("API key missing: set the environment variable …")`.
  The message names the variable, so you know what to set. It never contains a key.
- The key is never in the code or the repository, as `PROJEKT.md` requires.

The DoD point "reported **before the first request**" is met in part 2: the Alpha Vantage source
reads the key in its constructor, so a missing key fails before any `get` is called. A test will
check that `fake.anfragen()` is empty.

## 7. `FakeHttpClient` (`tests/fake_http_client.hpp`)

A test-only implementation of `IHttpClient`. It lives in `tests/`, not `src/`, because production
code must never use it.

| Method | Purpose |
|---|---|
| `setzeAntwort(url, status, body)` | this URL returns this answer |
| `setzeAusfall(url, Ausfall::Netzwerk / Zeitueberschreitung)` | this URL throws the matching error |
| `anfragen()` | list of all requested URLs, e.g. to check "no request was made" |
| `get(url)` | implements the interface (`override`) |

An **unknown URL** throws `NetzwerkFehler`. That behaves like an unreachable server, and it makes
typos in test URLs visible immediately instead of silently returning an empty answer.

The class is written entirely in the header (methods defined inside the class). That's fine for a
small test helper and means no extra `.cpp` in `CMakeLists.txt`.

## 8. Tests (`tests/test_quellen.cpp`)

| Test | Checks |
|---|---|
| `Quellenfehler.HierarchieErlaubtAllgemeinesUndGezieltesFangen` | every error is a `QuellenFehler`; a timeout is also a `NetzwerkFehler` |
| `Quellenfehler.KontingentIstKeinNetzwerkfehler` | a `catch (NetzwerkFehler)` doesn't catch a quota error |
| `Quellenfehler.HttpFehlerKenntStatus` | status and message are stored |
| `PruefeStatus.ErfolgWirftNicht` | 200 and 204 are OK |
| `PruefeStatus.UngueltigerSchluessel` | 401 → `HttpFehler`, message mentions the symbol and the API key; 403 too |
| `PruefeStatus.NichtGefunden` | 404 → `HttpFehler` with status 404 |
| `PruefeStatus.KontingentErschoepft` | 429 → `KontingentFehler` |
| `PruefeStatus.SonstigerFehlerstatus` | 500 → `HttpFehler` with status 500 |
| `FakeHttpClient.LiefertFestgelegteAntwortUndProtokolliert` | answer is returned, request is logged; called through `IHttpClient&` |
| `FakeHttpClient.FehlerstatusWirdZurueckgegeben` | 404 is returned, not thrown (the contract) |
| `FakeHttpClient.SimulierteAusfaelle` | network error, timeout, unknown URL |
| `ApiSchluessel.WirdAusUmgebungGelesen` | a set variable is read |
| `ApiSchluessel.FehlendeVariable` | missing → `KonfigurationsFehler`, message names the variable |
| `ApiSchluessel.LeereVariable` | empty counts as missing |

The API key tests use `setenv`/`unsetenv` (POSIX, available in the Linux container) with a
variable that only the tests use, and they clean up afterwards.

## 9. Result

`cmake -B build && cmake --build build && ctest --test-dir build`: 87 of 87 tests pass (14 new),
no compiler warnings.

## 10. Next: part 2

1. Add `third_party/nlohmann/json.hpp` (single header) and register the include path in CMake.
2. `AlphaVantageQuelle : IPriceSource`: reads the key in the constructor, builds the URL, calls
   `IHttpClient::get`, `pruefeStatus`, detects the quota note in a 200 answer, parses the JSON into
   `Kerze` objects (sorted ascending) and maps JSON errors to `ParseFehler`.
3. Fixtures in `tests/fixtures/`: one recorded real answer (without the key) and small hand-written
   files for each error case.

## 11. Possible exam questions

- *What is an interface in C++?* → A class with only pure virtual functions (`= 0`). It can't be
  instantiated; derived classes must implement all of them.
- *Why a virtual destructor?* → So that deleting through a base-class pointer calls the derived destructor.
- *Why does `get` return 404 instead of throwing?* → Keeps implementations simple and puts the
  interpretation in one place (`pruefeStatus`).
- *Why is `ZeitueberschreitungFehler` derived from `NetzwerkFehler`?* → A timeout is a kind of
  network error; code handling network errors handles it automatically.
- *Why isn't `KontingentFehler` an `HttpFehler`?* → Alpha Vantage reports quota exhaustion with HTTP 200.
- *Why is the fake in `tests/`?* → It's test infrastructure; production code must not depend on it.
- *What does `override` do?* → The compiler checks that the function really overrides a virtual
  function of the base class (catches typos in the signature).
- *Why read the key from an environment variable?* → So it never ends up in the repository.
