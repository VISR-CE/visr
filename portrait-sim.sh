#!/bin/bash
# Portrait play in the iPhone Duo simulator: display.allow_portrait + debug.orientation portrait,
# one player on Blood Gulch slayer through debug.network_test, once per debug.portrait_field_of_view mode.
#   portrait-sim.sh WORKTREE OUT [MAPS_DIR]
# Without MAPS_DIR, the first folder in the colon-separated VISR_MAPS_DIRS that holds ui.map is used.
# Inner-display screenshots at fixed times per mode; runtime logs per mode.
# It deletes only the simulator it created.
set -u
tree=$1 out=$2 maps=${3:-}
if [ -z "$maps" ]; then
	# VISR_MAPS_DIRS: colon-separated folders to search for a maps folder that holds ui.map.
	old_ifs=$IFS; IFS=:
	for candidate in ${VISR_MAPS_DIRS:-}; do
		IFS=$old_ifs
		[ -f "$candidate/ui.map" ] && { maps=$candidate; break; }
	done
	IFS=$old_ifs
fi
[ -d "$out" ] && find "$out" -mindepth 1 -delete; mkdir -p "$out"; out=$(cd "$out" && pwd -P); exec > >(tee "$out/run.txt") 2>&1
capped() {
	local limit=$1 pid watcher status; shift
	"$@" & pid=$!
	( sleep "$limit"; kill -TERM "$pid" 2> /dev/null; sleep 3; kill -KILL "$pid" 2> /dev/null ) & watcher=$!
	wait "$pid" 2> /dev/null; status=$?
	kill "$watcher" 2> /dev/null; wait "$watcher" 2> /dev/null
	[ $status -ge 128 ] && status=124
	return $status
}
step() { echo "$(date +%T) $*"; }
[ -f "$maps/ui.map" ] || { echo "no maps at $maps"; exit 1; }
app=$tree/build/ios/app-simulator/Release-iphonesimulator/VISR.app
# the build can wipe untracked files in the worktree, including a relative OUT inside it: log elsewhere, then recreate OUT
blog=$(mktemp "${TMPDIR:-/tmp}/portrait-build.XXXXXX")
step build; (cd "$tree" && python3 tools/ios_build.py --simulator > "$blog" 2>&1) || { tail -n 40 "$blog"; echo "build failed"; exit 1; }
mkdir -p "$out"; cp "$blog" "$out/build.log"
# the build is only linker-signed (CODE_SIGNING_ALLOWED=NO); this simulator refuses that bundle ("code has no resources but signature indicates they must be present"), so seal it ad hoc
codesign --force --sign - --timestamp=none "$app" > "$out/codesign.txt" 2>&1 && codesign --verify --deep --strict "$app" >> "$out/codesign.txt" 2>&1 || { cat "$out/codesign.txt"; echo "codesign failed"; exit 1; }
bundle=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$app/Info.plist")
runtime=$(xcrun simctl list runtimes | grep --only-matching 'com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4' | head -n 1)
[ -n "$runtime" ] || runtime=com.apple.CoreSimulator.SimRuntime.iOS-27-2
udid=$(capped 60 xcrun simctl create "portrait-$$" com.apple.CoreSimulator.SimDeviceType.iPhone-Duo "$runtime") || { echo "create failed"; exit 1; }
cleanup() { capped 30 xcrun simctl shutdown "$udid" > /dev/null 2>&1; capped 60 xcrun simctl delete "$udid" > /dev/null 2>&1; step deleted; }
trap 'cleanup; exit 143' INT TERM HUP
trap cleanup EXIT
[ -n "${HEAVY_JOB_CLEANUP_FILE-}" ] && echo "xcrun simctl delete $udid > /dev/null 2>&1" >> "$HEAVY_JOB_CLEANUP_FILE"
boot_once() { capped 300 xcrun simctl boot "$udid"; capped 900 xcrun simctl bootstatus "$udid" -b > /dev/null 2>&1; }
step boot
if ! boot_once; then
  # launchd_sim "could not bind to session" is a wedged CoreSimulatorService; restart it only when no other simulator is booted
  if xcrun simctl list devices booted | grep --quiet '(Booted)'; then echo "boot failed (other simulators booted, not restarting the service)"; exit 1; fi
  echo "boot failed; restarting CoreSimulatorService and retrying once"
  capped 60 xcrun simctl shutdown "$udid" > /dev/null 2>&1
  killall com.apple.CoreSimulator.CoreSimulatorService 2> /dev/null; sleep 10
  boot_once || { echo "boot failed after service restart"; exit 1; }
fi
step install; capped 120 xcrun simctl install "$udid" "$app" || { echo "install failed"; exit 1; }
data=$(capped 30 xcrun simctl get_app_container "$udid" "$bundle" data)/Documents
# the app shows its import screen unless every campaign map is present (xiso_maps_ready), so copy the whole set
mkdir -p "$data/maps"; for map in "$maps"/*.map; do capped 300 cp -c "$map" "$data/maps/" 2> /dev/null || capped 600 cp "$map" "$data/maps/" || { echo "map copy failed: $map"; exit 1; }; done
for mode in ${MODES:-hold horizontal_minimum}; do
	sub=$out/$mode; mkdir -p "$sub"
	fov=$mode; [ "$mode" = horizontal_minimum ] && fov=""
	orient='orientation = "portrait"'; override=""
	# laptopl: landscape with a faked half-open laptop L (VISR_POSTURE_OVERRIDE), for screenshots
	[ "$mode" = laptopl ] && { fov=""; override=laptop-l; }
	cat > "$data/config.toml" <<TOML
[display]
upscaled_textures = false
allow_portrait = true

[debug]
$orient
portrait_field_of_view = '$fov'
network_test = 'host:bloodgulch:slayer'
network_test_players = 1
network_test_start = 20
TOML
	cp "$data/config.toml" "$sub/config.toml"
	rm -f "$data/ios-runtime.log" "$data/stderr.log"
	step "launch $mode"
	sleep 45
	launched=0
	for attempt in 1 2 3; do
		if SIMCTL_CHILD_VISR_POSTURE_OVERRIDE=$override capped 60 xcrun simctl launch --terminate-running-process "--stdout=$sub/stdout.log" "--stderr=$sub/stderr.log" "$udid" "$bundle" > "$sub/launch$attempt.txt" 2>&1; then launched=1; break; fi
		step "launch attempt $attempt failed"; sleep 30
	done
	[ "$launched" = 1 ] || { echo "launch failed"; capped 60 xcrun simctl spawn "$udid" log show --last 5m --predicate 'process == "VISR"' > "$sub/sim-log.txt" 2>&1; cp "$data/ios-runtime.log" "$sub/" 2> /dev/null; exit 1; }
	start=$SECONDS
	for at in 12 50 90 130; do
		while [ $((SECONDS - start)) -lt "$at" ]; do sleep 2; done
		for display in 1 3; do capped 60 xcrun simctl io "$udid" screenshot "--display=$display" "$sub/shot-$at-d$display.png" > /dev/null 2>&1 || step "shot $at d$display failed"; done
		step "$mode shot at $at s"
	done
	capped 30 xcrun simctl terminate "$udid" "$bundle" > /dev/null 2>&1
	cp "$data/ios-runtime.log" "$sub/ios-runtime.log" 2> /dev/null; cp "$data/stderr.log" "$sub/app-stderr.log" 2> /dev/null
	grep -h -i "orientation\|portrait\|geometry\|screen mode\|field of view" "$sub"/*.log > "$sub/lines.txt" 2> /dev/null; head -n 20 "$sub/lines.txt"
done
capped 30 xcrun simctl io "$udid" enumerate > "$out/displays.txt" 2>&1
