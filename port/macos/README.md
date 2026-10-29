# Native macOS host

The AppKit target uses the macOS SDK, SDL3's Cocoa window and Metal view, and the same compiled guest and Metal renderer as the other Apple targets. It requires an Apple silicon Mac running macOS 14 or later. Catalyst remains available through `tools/ios_build.py --mac`.

```sh
python3 tools/macos_build.py --renderer metal
```

`--local-routes` additionally lets the build run the unsigned `benchmark/local-routes` catalog that `tools/replay_check.py` writes (replay checks and whole-take window review); it bundles no recordings.

The development product is `build/macos/app-native/host/Release/VISR.app`, bundle ID `org.steverice.visr.native`. The build signs it ad hoc and verifies the completed bundle; it does not use an Apple signing identity or provisioning profile. `--sandbox on` signs that development build with the App Sandbox entitlements (`VISR-macOS.entitlements`), so the sandboxed app can be tried without an account. An ad hoc build does not use CloudKit. `--cloud-saves --team YOUR_TEAM_ID` signs with Apple signing and adds the shared iCloud save engine; the profile must allow the selected container. `--unsigned --cloud-saves` checks compilation without signing or account access. The player must turn on **iCloud Saves** in Network Setup and relaunch before sync starts. The target does not bundle benchmark recordings. Signed profile, recording, and shader feeds are on by default through the existing verification path (`docs/data-packs.md`); `--data-packs off` leaves them out. The native target does not include the interactive benchmark panel or CPU/GPU timing graph.

## TestFlight builds

The Mac app shares the iOS app's App Store Connect record (universal purchase), so a team-signed build defaults to the bundle ID `org.steverice.visr`. Team-signed builds always run in the App Sandbox and also declare the iCloud container `iCloud.org.steverice.visr`, which iCloud saves and shader contributions use when the build includes them.

```sh
python3 tools/macos_build.py --team TEAM_ID --signing-profile PROFILE_UUID \
  --data-pack-channel tester --app-store-package build/release/macos
```

`--signing-profile` names an installed Mac App Store distribution profile and signs manually, without provisioning updates. Without it, Xcode's automatic signing may create certificates and profiles in the developer account. The export writes an installer package signed with the team's Mac Installer Distribution certificate. The CI release workflow (`docs/releasing.md`) builds, checks and uploads the `macos` platform; this tool never uploads.

## Data and import

On first launch, choose an original Xbox PAL or NTSC-US ISO or XISO. The importer validates and stages the maps before replacing incomplete data. The original disc image stays unchanged. The app keeps a read-only, security-scoped bookmark to the chosen image (`disc-image.bookmark` in the data folder), so a later import reopens it without asking; if the image has moved, the open panel appears again.

Native Mac starts in a resizable window. Set `display.fullscreen = true` or `display.mode = "fullscreen"` for an explicit fullscreen launch; `display.mode = "windowed"` selects the window. `HALO_DISPLAY_MODE` and `HALO_FULLSCREEN` provide launch overrides. Keyboard, mouse, controllers, and audio use SDL. Command-Q and closing the game window use the game's quit path.

Local saves, configuration, recordings, and imported data go under Application Support/`<bundle ID>`. In a sandboxed build that folder is inside the app's container (`~/Library/Containers/<bundle ID>/Data/Library/Application Support/<bundle ID>`). `HALO_DATA_ROOT` overrides the location for a development run and must be absolute; a sandboxed build can use only a folder inside its container. The app does not move or overwrite Catalyst data.

The following probe tests real Foundation file publication and rollback with a synthetic readiness validator. It does not launch an application or extract a real disc image:

```sh
xcrun --sdk macosx clang -fobjc-arc -mmacosx-version-min=14.0 \
  -DHALO_NATIVE_MAC=1 -DHALO_IOS=1 \
  -I port/ios/host -I port/runtime -I port/runtime/include \
  tools/macos_import_test.m -framework AppKit -framework UniformTypeIdentifiers \
  -o /tmp/visr-macos-import-test
/tmp/visr-macos-import-test
```

For AirPlay video, use macOS Control Center's **Screen Mirroring** controls.
Choose mirroring or an extended display, then move the VISR window to that
display or use the normal full-screen controls. The Mac app keeps its regular
window sizing and rendering settings; it does not use iPhone/iPad companion
screens or external-display profiles. For audio alone, choose an AirPlay
output in macOS Sound settings. Wireless video and audio can be delayed;
use a cable or local audio when response time matters. If a receiver drops,
check the game window on the Mac and select a working sound output.

## Discord account-linking trial

The trial scaffolding is opt-in:

```sh
python3 tools/ios_build.py --native-mac --sandbox on --discord on
```

`--discord on` sets `HALO_DISCORD`, adds the Discord callback scheme to the
built plist before signing, compiles the Keychain helpers and presence value
producer, and adds Discord Settings to the app menu. Link Discord remains
disabled until the Social SDK is integrated. The preflight does not open a
browser, store tokens, or publish presence. The C producer accepts public map
and mode labels, player counts, and a session start time; the live game adapter
is still pending. Ordinary builds default to `--discord off`.

Obtain the Discord Social SDK **C++ macOS arm64 release** archive from the
Developer Portal. It must include `discordpp.h`,
`libdiscord_partner_sdk.dylib`, supporting headers, and its license. Set
`VISR_DISCORD_SDK` to the local archive path or an HTTPS URL and
`VISR_DISCORD_SDK_SHA256` to an independently recorded lowercase SHA-256.
The build rejects missing or mismatched digests and records the accepted
hash under ignored `build/third_party/discord/`. It stages the archive without
extracting or linking it; inspect the actual package before adding those
steps. Never commit the SDK or a download URL containing credentials.

SDK linking, PKCE authorization, token exchange and refresh, live presence,
sandbox network access, binary size, and runtime entitlement requirements
remain unverified. The current trial does not add sandbox entitlements.
