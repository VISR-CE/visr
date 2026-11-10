# Security

## Supported versions

Only the latest commit on `main` gets fixes. There are no release branches
yet.

## Reporting a vulnerability

Please do not open a public issue for a security problem. Report it privately
through GitHub's
[private vulnerability reporting](https://github.com/VISR-CE/visr/security/advisories/new)
for this repository.

Include what you found, how to reproduce it, and what an attacker could do with
it. You should hear back within a week. Once a fix is in `main`, we will credit
you in the advisory unless you would rather we did not.

## What is in scope

VISR runs a game written in 2001 for a console that ran only signed code, so
its file and network parsers were not written to handle hostile input. The
reports we care most about are the ones that reach the device from outside:

- **Internet and LAN play**: the invite links, the lobby and signaling code in
  `port/linux/src/p2p*.c`, the MQTT brokers listed in
  `port/assets/network/brokers.txt`, and the game's own network messages
- **Disc import**: the XISO parser in `port/runtime/xiso.c` and the importer,
  which read files the player picks
- **Saves and settings**: `config.toml`, profiles and saved games in the app's
  Documents folder
- The memory model: anything that lets game code read or write outside its 4
  GB block of memory, or run code that was not in the signed app

Bugs in the original game's handling of its own map files are in scope when a
crafted disc image can trigger them.

Problems in third-party libraries should go to their projects first. Tell us
too if VISR ships an affected version (see [NOTICE.md](NOTICE.md)).
