// Host C++ unit test for the fork's coalesced, off-thread deferred engine pause.
//
// This proves the property that prevents the iOS dispose/stop App Hang
// (BUBBLEGUM-APP-2EV / 2ET; upstream flutter_soloud #485, regressed by #406):
// scheduling the "no voices left -> pause the audio device" work must NOT block
// the calling (Flutter UI) thread, even when the pause work itself is slow — on
// iOS soloud.pause() runs ma_device_stop() -> AURemoteIO::Stop() -> a blocking
// mach_msg HAL round-trip that can take seconds. It must also fire when the
// engine is still idle, skip when a voice resumed during the settle window, and
// coalesce a burst (a screen teardown disposing N sources) down to ONE real
// device pause instead of N stop/start churns.
//
// Build + run on the host (no audio device / NDK needed):
//   clang++ -std=c++17 -pthread test/native/device_pause_test.cpp -o /tmp/dp && /tmp/dp
//
// RED  (against a naive synchronous schedule() that runs pauseWork inline):
//      test 1 blocks for the full work duration -> `took > budget` fails, and
//      test 4 runs pauseWork N times instead of coalescing to 1.
// GREEN (against DeferredEnginePause): schedule() returns within ~budget; the
//      pause fires once, later, and only while still idle.
#include "../../src/soloud/src/backend/miniaudio/device_pause.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono;
using soloud_fork::DeferredEnginePause;

static long long elapsedMs(steady_clock::time_point start) {
  return duration_cast<milliseconds>(steady_clock::now() - start).count();
}

int main() {
  int failures = 0;

  // 1. schedule() must return immediately even when the pause work is SLOW
  //    (simulating a hung iOS ma_device_stop). This is the ANR property: the
  //    caller (UI) thread is never blocked by the device stop.
  {
    DeferredEnginePause pause;
    std::atomic<bool> ran{false};
    auto start = steady_clock::now();
    pause.schedule(
        milliseconds(10), [] { return true; },
        [&] {
          std::this_thread::sleep_for(milliseconds(2000));
          ran = true;
        });
    long long took = elapsedMs(start);
    if (took > 100) {
      printf(
          "FAIL nonblock: schedule() took %lldms — caller BLOCKED on the pause "
          "(the ANR bug)\n",
          took);
      failures++;
    }
    if (ran.load()) {
      printf("FAIL nonblock: pause work ran synchronously on the caller\n");
      failures++;
    }
    // Keep `pause`/`ran` alive until the slow worker finishes so it can't touch
    // freed test state.
    std::this_thread::sleep_for(milliseconds(2100));
    if (!ran.load()) {
      printf("FAIL nonblock: deferred pause work never ran\n");
      failures++;
    }
  }

  // 2. When still idle at fire time, the pause DOES run (once, after the delay).
  {
    DeferredEnginePause pause;
    std::atomic<int> runs{0};
    pause.schedule(milliseconds(20), [] { return true; },
                   [&] { runs.fetch_add(1); });
    std::this_thread::sleep_for(milliseconds(120));
    if (runs.load() != 1) {
      printf("FAIL fires: expected 1 run, got %d\n", runs.load());
      failures++;
    }
  }

  // 3. If a voice resumed during the settle window (shouldFire -> false), the
  //    pause is SKIPPED — the device is not needlessly stopped.
  {
    DeferredEnginePause pause;
    std::atomic<int> runs{0};
    pause.schedule(milliseconds(20), [] { return false; },
                   [&] { runs.fetch_add(1); });
    std::this_thread::sleep_for(milliseconds(120));
    if (runs.load() != 0) {
      printf("FAIL skip: pause ran despite shouldFire()=false (%d)\n",
             runs.load());
      failures++;
    }
  }

  // 4. A rapid burst (e.g. disposing N sources on a screen teardown) COALESCES
  //    to at most one real device pause — not N stop/start churns.
  {
    DeferredEnginePause pause;
    std::atomic<int> runs{0};
    for (int i = 0; i < 20; i++) {
      pause.schedule(milliseconds(30), [] { return true; },
                     [&] { runs.fetch_add(1); });
      std::this_thread::sleep_for(milliseconds(1));
    }
    std::this_thread::sleep_for(milliseconds(150));
    if (runs.load() != 1) {
      printf("FAIL coalesce: expected exactly 1 pause for the burst, got %d\n",
             runs.load());
      failures++;
    }
  }

  // 5. cancel() supersedes a pending pause (e.g. engine deinit before it fires).
  {
    DeferredEnginePause pause;
    std::atomic<int> runs{0};
    pause.schedule(milliseconds(40), [] { return true; },
                   [&] { runs.fetch_add(1); });
    pause.cancel();
    std::this_thread::sleep_for(milliseconds(120));
    if (runs.load() != 0) {
      printf("FAIL cancel: pause ran after cancel() (%d)\n", runs.load());
      failures++;
    }
  }

  if (failures == 0) {
    printf("ALL PASS (device_pause)\n");
    return 0;
  }
  printf("%d FAILURE(S)\n", failures);
  return 1;
}
