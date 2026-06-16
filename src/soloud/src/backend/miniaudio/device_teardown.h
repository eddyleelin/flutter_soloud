// Fork patch (bubblegum): time-bounded audio-device teardown.
//
// Closing an OS audio device that the OS has already disconnected/reclaimed —
// e.g. Android AAudio/OpenSL after a background device reclaim (flutter_soloud
// #126) — can block the calling thread for many seconds inside ma_device_stop /
// ma_device_uninit (the hang reported in flutter_soloud #333). When the caller
// is the Flutter UI (platform) thread, that block trips Android's 5s ANR
// watchdog (BUBBLEGUM-APP-2CQ and its latent resume-side sibling).
//
// `teardownWithinBudget` runs the (potentially hanging) teardown work on a
// worker thread and waits at most `budget` for it:
//   * finishes in time  -> returns true  (clean teardown)
//   * overruns the budget -> the worker is DETACHED (abandoned) and the call
//     returns false immediately. The abandoned worker keeps running until the
//     OS finally unblocks, then completes and frees its resources on its own.
// Either way the caller can never block longer than `budget`.
//
// CONTRACT: the resources captured by `work` must be owned exclusively by
// `work` (its own heap-allocated ma_device/ma_context), so an abandoned
// teardown can never race a subsequent init that allocates fresh handles. See
// the device/context handle swap in soloud_miniaudio_deinit / miniaudio_init.
#ifndef SOLOUD_FORK_DEVICE_TEARDOWN_H
#define SOLOUD_FORK_DEVICE_TEARDOWN_H

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <utility>

namespace soloud_fork {

inline bool teardownWithinBudget(std::function<void()> work,
                                 std::chrono::milliseconds budget) {
  // Promise-backed future: unlike std::async, its destructor never blocks, so
  // abandoning it on timeout is safe.
  auto done = std::make_shared<std::promise<void>>();
  std::future<void> finished = done->get_future();

  try {
    // Copy `work` into the worker (std::function is copyable) so the outer copy
    // stays valid for the catch-fallback below if thread creation throws.
    std::thread worker([work, done]() mutable {
      work();
      done->set_value();
    });
    worker.detach();
  } catch (...) {
    // Thread resources exhausted (extremely rare). Running synchronously here
    // may block, but it is strictly better than std::terminate during audio
    // teardown. The original `work` is still intact (we captured a copy above).
    work();
    return true;
  }

  return finished.wait_for(budget) == std::future_status::ready;
}

}  // namespace soloud_fork

#endif  // SOLOUD_FORK_DEVICE_TEARDOWN_H
