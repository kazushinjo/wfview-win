// iOS audio-session configuration (Bluetooth HFP mic + output).
//
// iOS does not let an app pick an audio device by name; routing is decided by
// the shared AVAudioSession. This configures a PlayAndRecord session that
// allows Bluetooth (HFP, so a Bluetooth headset's microphone can be used) and
// re-applies itself on route changes. Call once, early, on iOS only.

#ifndef WFVIEW_IOSAUDIOSESSION_H
#define WFVIEW_IOSAUDIOSESSION_H

#ifdef __cplusplus
extern "C" {
#endif

void configureIosAudioSession(void);

#ifdef __cplusplus
}
#endif

#endif // WFVIEW_IOSAUDIOSESSION_H
