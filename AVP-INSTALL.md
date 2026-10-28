# Installing VISR on Apple Vision Pro

VISR is a native Metal port of Halo: Combat Evolved for Apple Vision Pro and other Apple devices, built on the decompilation and the community ports listed under [Credits](README.md#credits).

## Requirements

- An Apple silicon Mac with Xcode 27, Python 3 and Homebrew (to build; not needed for the prebuilt IPA route)
- Apple Vision Pro on visionOS 2 or later, with Developer Mode enabled
- An Apple ID. A free account works; apps signed with it must be re-signed every 7 days
- A game controller. Looking and pinching does not play the game
- A disc image of your own copy of the original Xbox release of Halo: Combat Evolved

## Game files

You need to supply your own game files. This repository contains none.

1. Make a disc image (`.iso` or `.xiso`) of your own Xbox disc.
2. Check that it is one of these builds: NTSC-US, map build `01.10.12.2276`, or PAL, map build `01.01.14.2342`. PC, Custom Edition and Master Chief Collection files do not work.
3. Copy the image to the Vision Pro, or to iCloud Drive, so the Files picker can reach it. You import it from the app (see [Import the disc image](#import-the-disc-image)).

## Build

Start from a checkout of this repository.

```sh
brew install cmake ninja llvm lld sdl3 pkgconf
python3 tools/ios_build.py --visionos --team YOUR_TEAM_ID --bundle-id com.yourname.visr
```

The script downloads SDL, musl and the Khronos headers itself. Add your Apple account in Xcode > Settings > Accounts first, and use a bundle identifier of your own. The [build and install guide](port/ios/README.md) covers toolchain locations, options and troubleshooting, and the [Apple Vision Pro section](port/ios/README.md#apple-vision-pro) covers the visionOS build.

If launching shows "Could not reserve the game's memory arena", rebuild with `--extended-virtual-addressing`, which needs a paid developer team.

## Install on Apple Vision Pro

Either install your own build or sign the prebuilt IPA.

**Your own build.** Pair the headset with Xcode (Window > Devices and Simulators) and enable Developer Mode on the headset. List devices, then install and launch:

```sh
xcrun devicectl list devices
xcrun devicectl device install app --device DEVICE_UDID \
  build/visionos/app-device/Release-xros/VISR.app
xcrun devicectl device process launch --device DEVICE_UDID com.yourname.visr
```

**Prebuilt IPA.** Each [GitHub release](https://github.com/steverice/visr/releases) attaches `VISR-visionOS-unsigned.ipa` and its `.sha256` file. The IPA is unsigned and will not launch as downloaded. Verify the checksum, then sign and install it with your own Apple ID using a tool such as [AltStore](https://faq.altstore.io/) or [Sideloadly](https://sideloadly.io/faq). Keep the same account and bundle identifier when you update, and your saves carry over.

On first launch, trust the developer profile in Settings if visionOS asks.

## Import the disc image

1. Open VISR and tap **Choose XISO**.
2. Pick your `.iso` or `.xiso` in Files. Alternatively, copy one image into the VISR folder in Files before launching.
3. The app checks the image and its maps before copying, then extracts them and starts the game. Later launches go straight to the game, and you can delete the image.

## Controls and known issues

- A game controller plays. A keyboard's arrow keys, Return and Escape work the menus.
- The game opens in a window. On visionOS 26 or later, press **Enter VR** below the window to play in stereo around you, with your head turning the view. Press the Digital Crown to leave VR and return to the window; the game keeps running. With a game controller, hold View and Menu together for a second to switch in either direction.
- Advanced: `config.toml` (in the VISR folder in Files) changes what VR shows and where the game starts. `display.stereo = "screen"` makes Enter VR show the game in 3D on a theater screen instead of around you. `display.immersive = true` starts the game in the immersive space: a flat screen in your room with `display.stereo = "off"` (the default), or VR with `"head"` or `"screen"`. [ROADMAP.md](ROADMAP.md) names the setting behind each feature.
- Closing the window pauses the game. If visionOS reclaims the closed window, the game quits; it saves at checkpoints.
- PlayStation VR2 Sense controllers aim on visionOS 27 (`input.aim`).

## License

VISR is licensed under Apache-2.0 (see [LICENSE](LICENSE)). Some of its code originated in CC0 1.0 projects; [NOTICE.md](NOTICE.md) credits them and lists third-party components that keep their own licenses.
