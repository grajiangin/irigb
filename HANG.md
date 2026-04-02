# Hang & Freeze Analysis

System: ESP32-S3 + FreeRTOS + W5500 Ethernet + IRIG-B output + TM1668 display + AsyncWebServer

Symptom: At random intervals, the web server becomes inaccessible and the display freezes simultaneously.

---

## #1 — CRITICAL: No `ws->cleanupClients()` (Heap Exhaustion) ✅ FIXED

**File:** `lib/server/server.cpp`

`ws->textAll()` is called every 300ms but stale/zombie WebSocket connections are never cleaned up. Dead connections accumulate, their TX buffers pile up, and the heap slowly drains. Once the heap is exhausted, both the web server and all `String` allocations fail simultaneously — which explains both symptoms appearing together.

**Fix:** Call `ws->cleanupClients()` before `ws->textAll()` in `sendTimeUpdate()`.

---

## #2 — CRITICAL: `Settings`/NVS Concurrent Access Without Mutex

**Files:** `lib/settings/settings.cpp`, `lib/server/server.cpp`, `lib/ethernet/ethernet.cpp`, `src/main.cpp`

The `Settings` object and `Preferences` (NVS flash) are accessed simultaneously from 4 contexts with no mutex:
- `loop()` task
- `ntp_task` FreeRTOS task
- `eth_monitor_task` FreeRTOS task
- AsyncTCP task (WebSocket handlers)

`Preferences` is not thread-safe. Concurrent `begin()`/`end()` calls can deadlock the NVS flash subsystem, freezing anything that touches storage.

Additionally, `setNetworkChangesFlag(true)` internally calls `save()` (settings.cpp:87-93), but the caller also calls `save()` afterward. This triggers 3 consecutive NVS write cycles from the AsyncTCP callback context, making concurrent access collisions more likely.

---

## #3 — CRITICAL: Timer ISR `digitalWrite()` vs Display Bit-Bang GPIO Spinlock Contention

**Files:** `src/main.cpp:43-65`, `lib/display/display.cpp:56-66`, `lib/display/display.cpp:357-365`

The 0.5ms timer ISR calls `irigb1.update()` through `irigb8.update()`, each doing `digitalWrite()`.

`display.display()` does 364+ `digitalWrite()` calls per refresh via bit-banging (14 bytes × 26 GPIO operations each).

On ESP32-S3, `digitalWrite()` acquires an internal spinlock. The ISR fires every 0.5ms and will spin-wait on that lock while the display is refreshing. This creates severe priority inversion and can starve `loop()` entirely, causing the display to freeze and making the web server unresponsive.

---

## #4 — ~~HIGH: W5500 Hardware Reset Without Pausing Other Tasks~~ DEAD CODE

**File:** `lib/ethernet/ethernet.cpp:304-321`

`eth_reinit_flag` is declared `false` and never set to `true` anywhere in the codebase. The entire W5500 hardware reset block is unreachable. Planned recovery mechanism, never triggered. Not contributing to any hang.

---

## #5 — HIGH: `irig_available` Not `volatile` — ISR Sees Stale Value

**File:** `src/main.cpp:135`

`irig_available` is written by `ntp_task` and read inside the timer ISR `onTimer()`. It is declared as a plain `bool` with no `volatile` qualifier. The compiler may cache it in a register and the ISR never sees the updated value.

Same issue applies to `ntp_ok` and `ntp_valid` which cross task/ISR boundaries.

---

## #6 — HIGH: IRIGB Double-Buffer `use_buffer_0` Not `volatile` — ISR/Task Race

**File:** `lib/irig/irigb.cpp:28, 71, 110-115`

`ntp_task` calls `encodeTimeIntoBits()` which writes the back buffer. The timer ISR calls `update()` which reads the front buffer and flips `use_buffer_0`. The flag is not `volatile`. If the ISR flips it mid-write by the task, the ISR begins reading from the buffer actively being written — corrupted IRIG-B output and potential ISR misbehavior.

---

## #7 — HIGH: UDP `begin()` Called Repeatedly Without `stop()` — Socket Leak

**File:** `lib/ntp/ntp.cpp:116-117`

`ntp_begin(_port)` calls `ntpUDP->begin(_port)` without first calling `ntpUDP->stop()`. Repeated calls leak lwIP UDP sockets. Once the socket pool is exhausted, all network operations fail — web server, NTP, everything.

---

## #8 — ~~MEDIUM: `ntp_forceUpdate()` Blocks Up to 2 Seconds~~ NOT A REAL PROBLEM

**File:** `lib/ntp/ntp.cpp:62-81`

`delay(10)` inside the loop calls `vTaskDelay()` — it yields the task, not a busy-wait. Other tasks (AsyncTCP, loop, eth_monitor) run normally during the wait. The 2-second worst case only blocks `ntp_task` itself; IRIG-B ISR keeps running from last good buffer. No observable impact.

---

## #9 — MEDIUM: WiFi Stack Running Unnecessarily

**File:** `src/main.cpp:2`

`#include <WiFi.h>` is present and `WiFiUDP` is used for NTP. There is no `WiFi.mode(WIFI_OFF)` call. The WiFi radio and its background tasks are likely active, consuming ~40KB RAM and CPU time on core 0, competing with Ethernet/lwIP operations.

---

## #10 — MEDIUM: W5500 INT Delayed by Timer ISR

The W5500 uses SPI3_HOST and relies on its INT pin (GPIO 36) for event notification. The 2000Hz timer ISR is extremely aggressive. If it delays the W5500 interrupt handler long enough, the Ethernet driver misses events, causing the network stack to stall waiting for responses that were already received but never processed.

---

## #11 — MEDIUM: DHCP Blocks Up to 10 Seconds at Startup While ISR Runs

**File:** `lib/ethernet/ethernet.cpp:176-189`

`eth_dhcp()` blocks for `100 × 100ms = 10s` during `setup()`. The timer ISR is already running at this point (started at `src/main.cpp:162`, before ethernet init), doing `digitalWrite()` every 0.5ms during initialization.

---

## Summary Table

| # | Issue | Severity | Status |
|---|-------|----------|--------|
| 1 | No `ws->cleanupClients()` → heap exhaustion | CRITICAL | FIXED |
| 2 | `Settings`/NVS concurrent access, no mutex → flash deadlock | CRITICAL | Open |
| 3 | Timer ISR vs display bit-bang GPIO spinlock contention | CRITICAL | Open |
| 4 | W5500 reset without pausing other tasks | HIGH | Open |
| 5 | `irig_available` not volatile → ISR sees stale value | HIGH | Open |
| 6 | IRIGB double-buffer not volatile → ISR/task race | HIGH | Open |
| 7 | UDP `begin()` without `stop()` → socket leak | HIGH | Open |
| 8 | NTP blocks up to 2s in task | MEDIUM | Open |
| 9 | WiFi stack active unnecessarily | MEDIUM | Open |
| 10 | W5500 INT delayed by timer ISR | MEDIUM | Open |
| 11 | DHCP blocks 10s at startup while ISR runs | MEDIUM | Open |
