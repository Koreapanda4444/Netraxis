# Netraxis

[English](README.md) | [한국어](README.ko.md)

A C++20 / Qt 6 Widgets desktop application for unified network and internet management.

The foundation, persistence and platform framework stages (001–028) are implemented: application lifecycle, bounded asynchronous work, validated settings, privacy-safe event logging, network domain types, evidence metadata, SQLite migrations and repositories, saved workspace management, and observation retention controls.

The Overview displays the selected platform/provider, current process permission, per-feature support and collection results, observed interfaces and addresses, failure reasons, and snapshot freshness. Collection runs in the background with refresh/cancel controls. Available system event reports trigger a debounced refresh; these reports do not establish internet or service reachability.

Backend operations are serialized per coordinator. Cancellation and deadlines are cooperative: synchronous native calls run off the UI thread and cannot be forcibly interrupted. Normalization preserves source/time metadata and marks invalid optional fields as partial evidence. Configuration transaction hooks capture restorable state, bind approval to the desired and recovery requests, reject state drift, and distinguish applied, restored, pending recovery, and restoration failure results. Native configuration writes remain unsupported in this stage.

Basic interface and address inventory is available through Qt’s native interface provider. Detailed OS-specific collection and configuration writes follow in subsequent stages. The interface marks unavailable and unverified features explicitly.

## Build

Install a C++20 compiler, CMake 3.24+, Ninja, and Qt 6.4+ with Widgets, Network, SQL and the SQLite driver. Point `CMAKE_PREFIX_PATH` at your Qt installation if it is not detected automatically.

```sh
cmake --preset release
cmake --build --preset release
```

Run `build/release/netraxis` on Linux, `build/release/netraxis.exe` on Windows, or the `build/release/netraxis.app` bundle on macOS. Windows builds using the Qt MSVC package require an MSVC developer shell. CI configures, builds and launches the app on all three systems using Qt 6.8.3.

Use `--platform-report` to print the selected provider and observed interface count as JSON. It returns a nonzero exit code when the inventory cannot be read.

## Local data

Settings, the SQLite database and rotating application event logs use Qt's per-user application directories. Passwords and tokens are not written to the database or event log; device models hold opaque credential references.

Observation cleanup runs at startup and every 15 minutes while the app is open, using the saved age and storage budget. Profiles are preserved during this cleanup. Settings contains a separate confirmation-gated action to delete saved network records and migration backups. SQLite secure deletion, WAL truncation and database compaction are applied; this is not a guarantee of physical-media erasure or deletion of system backups.

Schema upgrades create a private recovery copy for existing supported databases. Cleanup removes obsolete backups; unknown or newer schema versions are rejected. Invalid settings are reported and do not activate automatic data cleanup until valid settings are saved.
