// Fork patch (bubblegum): Android audio backend selection.
//
// An AAudio stream-lifetime race during miniaudio startup on Android 11 and 12
// leaves AudioStream::waitForStateChange dispatching through an invalid stream:
// __cxa_pure_virtual on 64-bit or pc=0 on 32-bit (BUBBLEGUM-APP-3Q0, -3J8,
// -3K4, and -3NB). Use the mature OpenSL ES backend on affected API levels;
// retain AAudio on Android 13+.
#ifndef SOLOUD_FORK_ANDROID_AUDIO_BACKEND_H
#define SOLOUD_FORK_ANDROID_AUDIO_BACKEND_H

namespace soloud_fork {

constexpr int kLastAndroidApiRequiringOpenSL = 31;

constexpr bool shouldUseOpenSLForAndroidApi(int apiLevel) {
  return apiLevel <= kLastAndroidApiRequiringOpenSL;
}

}  // namespace soloud_fork

#endif  // SOLOUD_FORK_ANDROID_AUDIO_BACKEND_H
