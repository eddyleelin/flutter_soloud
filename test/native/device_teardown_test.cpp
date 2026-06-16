// Host C++ unit test for the fork's time-bounded teardown primitive.
//
// This proves the exact property that prevents the Android background-reclaim
// ANR (flutter_soloud #333 / #126): a teardown that the OS lets hang must NOT
// block the calling (UI) thread beyond a small budget.
//
// Build + run on the host (no audio device / NDK needed):
//   clang++ -std=c++17 -pthread test/native/device_teardown_test.cpp -o /tmp/dt && /tmp/dt
//
// RED  (against a naive synchronous teardown): test 2 blocks for the full work
//      duration -> `took > 400` fails.
// GREEN (against the bounded+abandon teardown): test 2 returns in ~budget.
#include "../../src/soloud/src/backend/miniaudio/device_teardown.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace std::chrono;
using soloud_fork::teardownWithinBudget;

static long long elapsedMs(steady_clock::time_point start) {
  return duration_cast<milliseconds>(steady_clock::now() - start).count();
}

int main() {
  int failures = 0;

  // 1. A fast teardown completes within budget: returns clean=true, the work
  //    actually ran, and the call returns promptly.
  {
    std::atomic<bool> ran{false};
    auto start = steady_clock::now();
    bool clean = teardownWithinBudget([&] { ran = true; }, milliseconds(500));
    long long took = elapsedMs(start);
    if (!clean) { printf("FAIL fast: expected clean=true\n"); failures++; }
    if (!ran.load()) { printf("FAIL fast: work did not run\n"); failures++; }
    if (took > 200) { printf("FAIL fast: took %lldms (>200)\n", took); failures++; }
  }

  // 2. A teardown the OS lets hang (simulated by a long sleep) must NOT block
  //    the caller: the call returns within ~budget, reports clean=false
  //    (abandoned), and the work has NOT finished at return time.
  {
    std::atomic<bool> finished{false};
    auto start = steady_clock::now();
    bool clean = teardownWithinBudget(
        [&] {
          std::this_thread::sleep_for(milliseconds(2000));
          finished = true;
        },
        milliseconds(100));
    long long took = elapsedMs(start);
    if (clean) { printf("FAIL slow: expected clean=false (abandoned)\n"); failures++; }
    if (took > 400) {
      printf("FAIL slow: took %lldms — caller BLOCKED on the hung teardown (the ANR bug)\n", took);
      failures++;
    }
    if (finished.load()) { printf("FAIL slow: work should not have finished yet\n"); failures++; }

    // 3. The abandoned worker still completes later, without crashing, even
    //    though nothing is waiting on it anymore.
    std::this_thread::sleep_for(milliseconds(2200));
    if (!finished.load()) { printf("FAIL slow: abandoned work never completed\n"); failures++; }
  }

  if (failures == 0) {
    printf("ALL PASS (device_teardown)\n");
    return 0;
  }
  printf("%d FAILURE(S)\n", failures);
  return 1;
}
