# Physical Switch Pro Controller — issue #202

## Status (2026-10-27)

**Experimental wired implementation, not PS5 hardware-qualified.** This is a real
userland USB transport and frontend connection, not just a report parser and not
the already-existing emulated guest Pro Controller setting. It does **not** close
the physical-controller acceptance gate: no controller, USB node, native-app
sandbox, rest-mode behavior or rumble/motion has been exercised on a console.

The original request is [upstream #202](https://github.com/Swordpdf/ProsperoEden/issues/202):
physical original/compatible Pro Controller input, with MissionControl mentioned
as inspiration. MissionControl's Switch kernel/service environment cannot be
imported into a PS5 app. No MissionControl code or binary is used here.

## What is implemented

- `headless/pro_controller/pro_controller_usb.c` uses the **existing FreeBSD USB
  filesystem API** in Payload SDK headers: `open`, `ioctl`, `USB_GET_DEVICE_DESC`,
  `USB_GET_FULL_DESC`, `USB_FS_INIT/OPEN/START/COMPLETE/UNINIT`. These are not
  invented Sony HID entry points. There are two persistent 64-byte transfers,
  one interrupt IN and one OUT, with nonblocking completion polling.
- Opt-in discovery of **one wired `057e:2009` device**. VID/PID is checked before
  any output or transfer setup; a bounded configuration-descriptor walk requires
  an unambiguous HID interface with 64-byte interrupt IN and OUT endpoints.
  No device reset, configuration change, interface-driver detach, Bluetooth HCI
  access, virtual-device injection or privilege modification is performed.
  Discovery reads only USB device descriptors on nonmatching ugen nodes.
- Bounded USB status/handshake/high-speed/force-USB setup; device type verification;
  matching subcommand ACKs; SPI **reads only** for factory/user stick calibration
  and factory IMU calibration; IMU/rumble enable; full `0x30` report mode.
  Commands have 150 ms deadlines and three attempts, inside a 6 s total setup
  budget (including a transport stuck busy). Optional unsupported
  calibration/feedback steps degrade safely; essential setup failures reconnect.
- Validated `0x30` full reports (minimum 49 bytes), and independently useful
  `0x3f` simple reports (minimum 12 bytes). Unknown IDs, oversized/truncated
  packets and invalid hats cannot update input or extend liveness. SPI replies
  must match address, length, ACK and subcommand. `0x21` ACKs never become input.
- Calibrated asymmetric 12-bit sticks, clamping and neutral centres, digital
  ZL/ZR, all normal game buttons and D-pad. Erased/implausible calibration is
  rejected atomically. Uncalibrated sticks use a conservative nominal range;
  startup never mistakes a held stick for its centre. The existing configured
  deadzone/remapping and Joy-Con-grip handling still apply in games.
- Motion only after valid factory calibration **and** IMU-enable ACK. Uses the
  newest of three sensor samples; bias/scale and axes normalize to the existing
  DualSense bridge (G and rad/s). Sensorless reports clear motion rather than
  leaving a previous angular velocity latched. No orientation fusion/NFC claim.
- Rumble only after its enable ACK: three conservative amplitude levels, fixed
  160/320 Hz carriers, at most one update per 50 ms. This is **coarse rumble**, not
  faithful HD frequency reproduction or per-side guest synthesis. Unsupported
  feedback is not advertised through `PadEngine::IsVibrationEnabled`.
- Missing/erroring devices release all input and reconnect with 1–30 s capped
  exponential backoff. Only validated input resets backoff. A 1 s input watchdog
  releases held buttons/sticks/motion even when malformed traffic continues.
  Each poll processes at most four reports, with no worker thread or growing
  queue. Normal teardown makes a bounded best-effort stop-rumble write before
  cancelling transfers; unplug/error teardown never waits for the missing device.
- Launcher (`radio_input.c`) and gameplay (`pad.cpp`) both consume the actual
  transport. A ready physical controller takes **player 1's input**, and the
  existing DualSense is the fallback when it leaves. They never feed the same
  player simultaneously; fallback reports are drained, not replayed on removal.
  Source transitions release held keys and shortcuts.
  Raw USB does not allocate players 2–4, which retain the signed-in-user route.

## Opt in for an attended console test

Create an **empty regular file** at:

```text
/data/prosperoeden/pro-controller-usb.enabled
```

Connect a data-capable USB cable and reopen the launcher/app. Removing that file
and reopening disables discovery. Symlinks/nonempty files do not enable it.
The host build always disables native USB. There is no new account, server,
pairing database, settings-schema field or UI translation string.

The incumbent ProsperoEden interface and DualSense glyphs are unchanged. Nintendo
A/B/X/Y retain their game labels through the default guest mapping. In the
launcher, the default **Cross/select is physical B**, **Circle/back is A**,
Square is Y, Triangle is X. Existing menu keybinds can change those actions.
Plus is Options, Minus is Create (the existing Minus fallback), Capture is the
touchpad/shortcut key. Thus Capture+L returns to the launcher, Capture+R toggles
the HUD, and Capture+Y opens the existing quick strip. Raw Home is deliberately
not injected into the PS5 system UI; retain a DualSense for console recovery.

Launcher shutdown releases the process-local USB owner before the gameplay pad
opens; gameplay `Close` releases it before the launcher starts again. No extra
thread outlives these existing ownership boundaries. Marker changes are observed
at those boundaries, not by another filesystem poll on every input tick.

## Transport evidence and remaining blockers

The pinned build uses Payload SDK **v0.42**, ZIP SHA-256
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
Its `target/include/dev/usb/usb_ioctl.h` defines the above USB structures and
ioctls. Its `libkernel` imports supply `open`, `ioctl`, `clock_gettime`, `close`
and `lstat`; the focused native link check resolves the remaining libc functions.
The SDK's distributed import libraries do **not** include `libSceUsbd.so`,
`libSceUsbHid.so` or a general Sony Bluetooth host library. This says what this
SDK exposes, not what every firmware exports internally. Generic USB FS is an
alternative, so absence of those SCE libraries alone is not proof that physical
USB is impossible.

The local GPL-3.0-or-later [AnyPad](https://codeberg.org/elmonomalva0/Any-Pad-ps5)
source at upstream `44b8312c9abaf4aaff03d0c74c62a4da010c0108` uses USB FS from PS5
payload userland (`src/hci_usb.c`, notably device-descriptor reads and
`USB_FS_INIT/OPEN/START/COMPLETE`). That is **source evidence for the API path**,
not evidence that a ProsperoEden **native application sandbox** can access an
external Pro Controller. Its Bluetooth implementation is not imported.

On a particular console, the genuine gate is all of:

1. `/dev` exposes the controller's `/dev/ugen<bus>.<address>` node to this app.
2. `open(..., O_RDWR)` and descriptor reads are permitted; a `/data` mount or
   successful font/filesystem elevation alone does not prove USB permission.
3. `USB_FS_INIT` and `USB_FS_OPEN` allow the two HID endpoints while any incumbent
   driver is attached. This implementation **refuses to detach that driver**.
4. Nintendo setup replies and continuous full reports arrive on the actual unit.

`EDEN_PRO_USB` logs identify discovery, denied-node counts, descriptor rejection,
the exact failed operation and `errno`, endpoint transfer status, connection
changes and enabled capabilities. For example `opendir-dev`, denied nodes,
`USB_GET_FULL_DESC`, `USB_FS_INIT`, or `USB_FS_OPEN-no-driver-detach` is an actionable
gate, not a fabricated success. If the native app lacks ugen exposure/permission
or a driver owns those endpoints, a separately designed platform/driver grant is
required. No such grant is bundled or claimed; console logs are required to
identify which gate actually fails on that environment.

**Bluetooth remains unimplemented.** A safe direct Bluetooth host must preserve
all events needed by Sony's existing DualSense stack. The local study
`Documentation/any-pad/2026-10-26-ultimate-2.4g-dualsense-logoff-study.md:13–25`
documents source-level event-reader contention and reported coexistence problems
in AnyPad; it explicitly distinguishes those from hardware proof. Taking over
the internal radio or porting its resident virtual-pad stack is not an acceptable
silent fallback. Externally managed adapters/bridges that already appear through
`scePad` continue to use the existing route, without a compatibility guarantee here.

Other limits: first matching device only; no new VID/PID guessing or arbitrary
clone profiles; no wireless pairing/reconnection; no battery UI, player LEDs or
NFC; no user IMU override; one newest IMU sample/report rather than all three;
coarse mirrored rumble; no guarantee a teardown stop packet reaches unplugged
hardware. A clone that cannot ACK full report mode or emits only change-triggered
simple reports will not meet this continuous USB session contract.

## Tests and reproduction

`tools/check-pro-controller.sh` is standalone and does **not** build the emulator.
It runs ASan+UBSan checks over synthetic, explicitly non-recorded fixtures:

- every full/simple report truncation, unknown IDs, oversized packets, invalid
  hats, all face buttons, opposite D-pad directions, digital triggers;
- centres/extremes, asymmetric factory calibration, user magic, invalid/erased
  calibration and atomic rejection, calibrated sensor axes and bridge ABI;
- descriptor lengths, malformed entries, wrong class/alternate setting/packet
  size/direction; a Bluetooth-class interface is rejected;
- complete setup, matched/mismatched SPI replies, optional-feedback NACKs,
  essential timeout/retry budget, stale/malformed input, disconnect/reconnect,
  neutralization, stop rumble, rate limit and capped discovery backoff;
- real launcher C input loop with mocked transports: USB-only init, held-key
  release, reconnect/source switching, controller mask, timed rumble, oversized
  SDK sample counts, error neutralization and DualSense fallback;
- 100,000 deterministic bounded random report/descriptor/calibration inputs.

With `PS5_PAYLOAD_SDK` set, it also compiles the protocol, actual USB transport
and launcher input objects for `x86_64-sie-ps5`, with warnings treated as errors;
it links a small native transport symbol smoke module, **not a packaged app**.
With `EDEN_SOURCE`, `BOOST_SOURCE`, `FMT_SOURCE` also set, it compiles the **real
modified `pad.cpp`** against the pinned Eden/Boost/fmt and SDK headers, without
building other emulator targets. No header mocks are used for that check.

The checks passed in `prosperoeden-host:features`, `--cpus=2 --memory=2g`, with
the checkout read-only and only `prosperoeden-team-pro-controller-20261027`
mounted writable. Dependencies were fetched/hash-verified only in that isolated
volume: Eden `67bada77`, fmt 12.1.0, Boost 1.92.0 from Eden's pinned cpmfile, and
SDK v0.42. The helper accepts output override `PRO_CONTROLLER_TEST_OUT`.

```powershell
docker run --rm --cpus=2 --memory=2g --network=none --entrypoint bash `
  --mount 'type=bind,src=<worktree>,dst=/checkout,readonly' `
  --mount 'type=volume,src=prosperoeden-team-pro-controller-20261027,dst=/work' `
  -e PS5_PAYLOAD_SDK=/work/pro-controller/sdk-v0.42/ps5-payload-sdk `
  -e EDEN_SOURCE=/work/pro-controller/source/.deps/mirror-67bada77f8a43a90da2e94e89b8e7da73c256989 `
  -e BOOST_SOURCE=/work/pro-controller/boost/boost-1.92.0 `
  -e FMT_SOURCE=/work/pro-controller/source/.deps/fmt-12.1.0 `
  prosperoeden-host:features /checkout/tools/check-pro-controller.sh
```

Still **NOT RUN**: full integrated app build/package, original or clone physical
controller execution, USB access from the app sandbox, real report captures,
motion orientation/rumble feel, launch/game/launcher transitions on PS5,
disconnect/replug during held input, rest/resume, multiple USB devices, and
coexistence with all signed-in users. Do not mark #202 complete from these host
or compiler results.

## References and licensing

The new code is independently authored GPL-3.0-or-later, compatible with this
repository. No vendor headers, MissionControl binaries, AnyPad source or SDL
driver implementation is copied into it. Protocol constants/field layouts are
interoperability facts cross-checked against:

- [SDL 3.2.0 Switch HID driver](https://github.com/libsdl-org/SDL/blob/release-3.2.0/src/joystick/hidapi/SDL_hidapi_switch.c),
  **zlib license**, Sam Lantinga/SDL contributors and Valve: report layouts,
  SPI calibration ordering, USB handshake, neutral/limited rumble values and
  `SendSensorUpdate` axis conventions. SDL is a compatible possible future
  backend but is not a PS5 native transport by itself.
- [Payload SDK v0.42](https://github.com/ps5-payload-dev/sdk/tree/v0.42),
  GPL-3.0-or-later SDK with **BSD-licensed FreeBSD headers**, in particular
  `include/freebsd/dev/usb/usb_ioctl.h` (FreeBSD releng/11.4). The build includes
  the SDK's existing headers rather than duplicating kernel structs/ioctl numbers.
- [AnyPad 0.9.5-beta source](https://codeberg.org/elmonomalva0/Any-Pad-ps5/src/commit/44b8312c9abaf4aaff03d0c74c62a4da010c0108),
  **GPL-3.0-or-later**, elmonomalvad0: evidence that PS5 payloads use USB FS,
  and a warning about why radio coexistence cannot be assumed. It is not added
  as a runtime dependency or imported as a binary.

No new gettext msgids. Shared merge surfaces: `headless/CMakeLists.txt`,
`headless/devices.h`, `headless/pad.cpp`, `headless/prosperoeden/radio_input.c`.
All other changes are feature-local. Aggregate release/integration documents and
the existing UI identity were not changed.
