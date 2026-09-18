# Walkie

Realtime walkie-talkie: mic → WebRTC AEC3 → UDP, and UDP → speaker. Learning project. Two devices on the same LAN (Mac CLI and/or iPhone). Not WAN.

## Mental model

Each side **listens** on a local UDP port and **sends** to the other host:port. There is no TCP session. Discovery (later) only has to fill in that address.

Four queues, 10 ms frames (160 samples, 16 kHz mono int16):

| Queue | Source | Sink |
|---|---|---|
| `nearEnd` | mic | AEC3 capture |
| `farEnd` | mixer tap (what you hear) | AEC3 reverse / echo reference |
| `output` | AEC3 | UDP send |
| `pback` | UDP recv | speaker |

AEC3 runs only when **near and far both have a full frame** (lockstep). Playback is `AVAudioPlayerNode` on a 10 ms timer, not `AVAudioSourceNode` (that traps on CoreAudio’s IO thread).

Far-end is the **mixer tap**, not the receive queue. Recv goes to the speaker first; the tap is what actually played.

## Layout

```
Sources/AudioProcessingCpp/   C++: AudioBridge, AEC3, Sender/Receiver, Endpoint
Sources/WalkieCore/           WalkieSession (AVAudioEngine + Network)
Sources/Walkie/               Mac CLI
Apps/WalkieiOS/               iPhone app (SwiftUI, links WalkieCore)
tools/                        offline AEC experiments
```

WebRTC is a **prebuilt** `webrtc-audio-processing` at:

`/Users/hifzur/Projects/webrtc-audio-processing/pre_install/{mac,ios}/arm64`

`Package.swift` points at that path. Rebuild the iOS `.a` if you change the iOS library; the Mac CLI uses `mac/arm64`.

Any Swift target that imports C++ needs `.interoperabilityMode(.Cxx)` (SPM) or `-cxx-interoperability-mode=default` (Xcode app target). Missing that is `'cstdint' file not found`.

## Run

**Mac, two processes (same machine):**

```bash
swift run Walkie 5001 localhost 5002
swift run Walkie 5002 localhost 5001
```

**Mac ↔ iPhone, same Wi‑Fi, no VPN.** This Mac’s Wi‑Fi IP is `ipconfig getifaddr en0`. The phone app shows its LAN IP.

Phone: listen `5000`, other IP = Mac, port `5000`, Start. Allow Mic + Local Network.

```bash
swift run Walkie 5000 <phone-ip> 5000
```

Allow incoming connections if macOS asks.

**iPhone app:** open `Apps/WalkieiOS/WalkieiOS.xcodeproj`, Team = Personal Team, run on a **physical** iPhone (the iOS WebRTC lib is device arm64, not Simulator). First launch: Settings → General → VPN & Device Management → Trust. Rebuild the app from Xcode after `WalkieCore` changes; `swift run` is Mac only.

## Processing

On send: HPF, AEC3, WebRTC NS (`kHigh`), AGC, limiter. That NS is stationary hiss/fan, not café isolation.

Do **not** enable `echo_canceller.export_linear_aec_output` with this WebRTC build at 16 kHz — AEC3 `BlockFramer` crashes.

An energy **gate** was tried and removed: in a loud room speech is not louder than the noise, so it mutes you.

Apple Voice Processing / Voice Isolation was tried and reverted. VPIO is a duplex I/O unit that must start at the hardware rate; our 16 kHz player graph throws `-10875` (`kAUInitialize` on the output node). Isolation in loud rooms later is either Apple VP (give up AEC3 on that device) or a neural NS after AEC in the 10 ms loop.

## Pitfalls worth remembering

- **IO thread:** HAL render callbacks (`AVAudioSourceNode`) cannot call into this Swift/C++ session. Player node + timer is the safe clock for `pback`.
- **Converter format:** `AVAudioConverter` asserts if the buffer’s format object is not `isEqual` to the format it was created with (`FillComplexProc`). The mixer tap is installed **after** `engine.start()` using that mixer format; incoming buffers are staged into `converter.inputFormat` before convert.
- **C++ interop** on every Swift target that sees `AudioProcessingCpp`, including the Xcode app target.
- No wavs in git (`*.wav` is ignored).
- Personal Team signing; the phone must trust the developer cert.

## Next seams (not done)

- Discovery (Bonjour is reserved in Info.plist as `_walkie._udp`) should only construct `Endpoint(host, port)` and pass it into `Network`. Send/recv/AEC stay the same.
- Loud-room voice isolation: neural NS on `output` after AEC, or Apple VP with AEC3 off.
