# Netraxis

[English](README.md) | [한국어](README.ko.md)

A C++20 / Qt 6 Widgets desktop application for unified network and internet management.

The foundation and persistence stages (001–019) are implemented: application lifecycle, bounded asynchronous work, validated settings, privacy-safe event logging, network domain types, evidence metadata, SQLite migrations and repositories, saved workspace management, and observation retention controls.

Native network collection and device configuration are subsequent stages. The interface marks unavailable and unverified features explicitly.

## Build

Install a C++20 compiler, CMake 3.24+, Ninja, and Qt 6.4+ with Widgets, Network, SQL and the SQLite driver. Point `CMAKE_PREFIX_PATH` at your Qt installation if it is not detected automatically.

```sh
cmake --preset release
cmake --build --preset release
```

Run `build/release/netraxis` on Linux, `build/release/netraxis.exe` on Windows, or the `build/release/netraxis.app` bundle on macOS. Windows builds using the Qt MSVC package require an MSVC developer shell. CI configures, builds and launches the app on all three systems using Qt 6.8.3.

## Local data

Settings, the SQLite database and rotating application event logs use Qt's per-user application directories. Passwords and tokens are not written to the database or event log; device models hold opaque credential references.

Observation cleanup runs at startup and every 15 minutes while the app is open, using the saved age and storage budget. Profiles are preserved during this cleanup. Settings contains a separate confirmation-gated action to delete saved network records and migration backups. SQLite secure deletion, WAL truncation and database compaction are applied; this is not a guarantee of physical-media erasure or deletion of system backups.

Schema upgrades create a private recovery copy for existing supported databases. Cleanup removes obsolete backups; unknown or newer schema versions are rejected. Invalid settings are reported and do not activate automatic data cleanup until valid settings are saved.
