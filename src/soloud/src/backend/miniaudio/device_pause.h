#ifndef SOLOUD_FORK_DEVICE_PAUSE_H
#define SOLOUD_FORK_DEVICE_PAUSE_H

// Coalesced, off-the-caller-thread engine pause for the auto-pause-when-idle
// path (Player::setPause / Player::stop / Player::disposeSound).
//
// THE BUG (BUBBLEGUM-APP-2EV / 2ET; upstream flutter_soloud #485, regressed by
// PR #406): those call sites historically ran `soloud.pause()` SYNCHRONOUSLY
// when the last active voice went away. On iOS `soloud.pause()` ->
// soloud_miniaudio_pause() -> ma_device_stop() -> AURemoteIO::Stop() -> a
// blocking mach_msg round-trip to the audio HAL, ON THE CALLING THREAD. Those
// sites are reached from Dart through a synchronous FFI prologue, i.e. on the
// Flutter UI thread, so under audio-server contention the stop blocks the UI
// thread for seconds -> App Hang / ANR. (Apple: AudioOutputUnitStop is always
// synchronous and waits on the HAL — rdar://666667 "AudioOutputUnitStop will be
// stuck in the main thread call"; miniaudio #400 blocks on ma_event_wait.)
//
// THE FIX (matches upstream PR #486 "wait some ms to pause device", hardened):
// run the device pause on a worker thread after a short settle delay, and only
// if the engine is still idle at fire time. The caller returns immediately and
// is NEVER blocked by the slow device stop. Rapid stop/dispose bursts are
// COALESCED via a generation counter: each schedule() supersedes the previous
// pending pause, so a screen teardown that disposes N sources performs at most
// ONE real device stop, and a stop->play within the settle window cancels the
// pause entirely (the device is never needlessly stopped+restarted, which also
// dodges the #446 stale-buffer-on-restart glitch the fork already avoids on
// Android/Web by keeping the device running).
//
// Lifetime safety: the generation counter lives in a heap cell owned by a
// shared_ptr that each worker copies, so a worker never dereferences `this`
// after the sleep — destroying the DeferredEnginePause (or the owning Player)
// while a pause is pending is safe (the destructor bumps the generation, so any
// still-sleeping worker sees a newer generation and exits without firing). The
// caller-supplied `shouldFire`/`pauseWork` closures must outlive a pending
// pause; in the Player they capture the process-lifetime engine, and the
// destructor's generation bump prevents them running after teardown.
//
// Header-only so it can be unit-tested on the host with no audio device:
//   clang++ -std=c++17 -pthread test/native/device_pause_test.cpp -o /tmp/dp && /tmp/dp

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace soloud_fork {

class DeferredEnginePause {
 public:
  DeferredEnginePause() : mGeneration(std::make_shared<std::atomic<uint64_t>>(0)) {}

  // Bump the generation so any pending-but-not-yet-fired worker sees a newer
  // value and returns without touching the (now-destroyed) closures.
  ~DeferredEnginePause() { cancel(); }

  DeferredEnginePause(const DeferredEnginePause &) = delete;
  DeferredEnginePause &operator=(const DeferredEnginePause &) = delete;

  // Schedule `pauseWork` to run after `delay`, but ONLY if it is still the most
  // recent schedule() at fire time (coalescing) AND `shouldFire()` returns true
  // then (the engine is still inited and no voice has resumed). Returns
  // immediately; `pauseWork` never runs on the calling thread.
  void schedule(std::chrono::milliseconds delay,
                std::function<bool()> shouldFire,
                std::function<void()> pauseWork) {
    auto generation = mGeneration;  // shared_ptr copy: outlives `this` + worker.
    const uint64_t myGen =
        generation->fetch_add(1, std::memory_order_acq_rel) + 1;
    std::thread([generation, myGen, delay, shouldFire = std::move(shouldFire),
                 pauseWork = std::move(pauseWork)]() {
      std::this_thread::sleep_for(delay);
      // Superseded by a newer schedule(), a cancel(), or destruction.
      if (generation->load(std::memory_order_acquire) != myGen) return;
      // Engine torn down, or a voice resumed during the settle window.
      if (!shouldFire()) return;
      pauseWork();  // the slow ma_device_stop, OFF the caller thread.
    }).detach();
  }

  // Supersede any pending-but-not-yet-fired pause (e.g. on engine deinit). Does
  // not interrupt a pause already past its shouldFire() check.
  void cancel() { mGeneration->fetch_add(1, std::memory_order_acq_rel); }

 private:
  std::shared_ptr<std::atomic<uint64_t>> mGeneration;
};

}  // namespace soloud_fork

#endif  // SOLOUD_FORK_DEVICE_PAUSE_H
