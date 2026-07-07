// iOS audio-session configuration (Bluetooth HFP mic + output). See header.

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include "iosaudiosession.h"

// Apply a PlayAndRecord category that allows Bluetooth, and prefer a Bluetooth
// HFP input when one is connected. Guarded so it does not re-fire in a loop:
// it only re-sets the category when the desired options are not already active.
static void wfApplyBluetoothSession()
{
    AVAudioSession *session = [AVAudioSession sharedInstance];
    NSError *err = nil;

    AVAudioSessionCategoryOptions wanted =
        AVAudioSessionCategoryOptionAllowBluetooth      // HFP: enables a BT mic
      | AVAudioSessionCategoryOptionAllowBluetoothA2DP  // high-quality BT output
      | AVAudioSessionCategoryOptionDefaultToSpeaker;   // fallback when no BT/headset

    if (![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord]
        || (session.categoryOptions & wanted) != wanted) {
        if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                      withOptions:wanted
                            error:&err]) {
            NSLog(@"wfview: AVAudioSession setCategory failed: %@", err);
            err = nil;
        }
    }

    // If a Bluetooth hands-free device is present, make its mic the input.
    for (AVAudioSessionPortDescription *input in session.availableInputs) {
        if ([input.portType isEqualToString:AVAudioSessionPortBluetoothHFP]) {
            if (![session setPreferredInput:input error:&err]) {
                NSLog(@"wfview: AVAudioSession setPreferredInput failed: %@", err);
                err = nil;
            }
            break;
        }
    }
}

void configureIosAudioSession(void)
{
    AVAudioSession *session = [AVAudioSession sharedInstance];
    NSError *err = nil;

    wfApplyBluetoothSession();

    if (![session setActive:YES error:&err]) {
        NSLog(@"wfview: AVAudioSession setActive failed: %@", err);
    }

    // Re-apply whenever the audio route changes (Bluetooth device connected /
    // disconnected, or another component such as Qt changing the category).
    [[NSNotificationCenter defaultCenter]
        addObserverForName:AVAudioSessionRouteChangeNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification * _Nonnull note) {
                    (void)note;
                    wfApplyBluetoothSession();
                }];
}
