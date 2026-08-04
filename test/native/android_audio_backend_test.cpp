// Host C++ unit test for the fork's Android audio backend cutoff.
//
// Build + run on the host (no audio device / NDK needed):
//   clang++ -std=c++17 test/native/android_audio_backend_test.cpp -o /tmp/aab && /tmp/aab
#include "../../src/soloud/src/backend/miniaudio/android_audio_backend.h"

#include <cstdio>

using soloud_fork::shouldUseOpenSLForAndroidApi;

int main() {
  int failures = 0;

  const auto expectOpenSL = [&](int apiLevel) {
    if (!shouldUseOpenSLForAndroidApi(apiLevel)) {
      std::printf("FAIL API %d: expected OpenSL ES\n", apiLevel);
      failures++;
    }
  };
  const auto expectAAudio = [&](int apiLevel) {
    if (shouldUseOpenSLForAndroidApi(apiLevel)) {
      std::printf("FAIL API %d: expected AAudio\n", apiLevel);
      failures++;
    }
  };

  // Android 10 already used OpenSL ES. Android 11 and 12 join that known-safe
  // path to avoid the AAudio start/reroute crash family.
  expectOpenSL(29);
  expectOpenSL(30);
  expectOpenSL(31);

  // Android 13+ keeps AAudio and its lower-latency path.
  expectAAudio(32);
  expectAAudio(35);

  if (failures == 0) {
    std::printf("ALL PASS (android_audio_backend)\n");
    return 0;
  }

  std::printf("%d FAILURE(S)\n", failures);
  return 1;
}
