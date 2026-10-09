# Eternal Launcher

<!-- impeccable:product-schema 1 -->

## Platform

Windows desktop native (Win32).

## Users

Community of a private Minecraft server. Confirmed by the owner: keep this audience and all current functions.

## Product Purpose

Prepare a private Minecraft 1.20.1 / Forge 47.4.10 instance and launch the game with a local nickname or Microsoft account.

## Capabilities and Constraints

C++20, Win32, GDI+, CMake. Preserve Microsoft device login, nickname validation and persistence, asynchronous manifest checks, portable Java installation, verified downloads, Forge installation, mod synchronization, launch and process monitoring. News remains demonstration content. No invented server player counts or gameplay connection status.

While preparing the game, a dedicated left panel replaces the landscape/status/news area and shows six stages (checks/Java, Minecraft/assets, natives, Forge, mods/configuration, final validation). Transfers show real bytes, current-transfer percentage when known, a five-second rolling speed estimate, conditional remaining-transfer time, and verified/reused file counts. Cancellation stops preparation and prevents launch, preserving verified complete files and worlds; Continue/Retry refreshes manifests and checks the cache, restarting partial files. Errors remain inline with recovery guidance and an installation-log shortcut. Successful preparation launches Minecraft automatically. Closing during preparation defers window destruction until worker cleanup. Authentication and launcher self-update retain their separate workflows.

Settings provide initial and maximum RAM (512–65536 MiB, initial no greater than maximum), window resolution and fullscreen, automatic Java 17 or a validated custom Java 17 executable, an exclusive game-instance directory, and optional launcher minimization on game launch with restoration after exit. Custom Java must include the console java.exe sibling used to prepare Forge. Changing the instance directory prepares another instance; existing worlds stay in their previous directory.

Maximum RAM also has a native slider in 0.5 GB steps and 4, 6 and 8 GB shortcuts. The interface displays Portuguese decimal commas and uses 1 GB = 1024 MiB; numeric fields retain precise integer MiB allocations. The slider starts at 0.5–16 GB and expands for larger existing or typed values. A slider/preset selection lowers an initial allocation that would exceed the new maximum.

Minimize launcher on game launch is enabled by default, including Restore defaults and preferences files without that property. A saved explicit false remains disabled. Restore defaults still changes only the draft until Save.

Preferences persist to UTF-8 JSON only through Save. Restore defaults changes the draft; Cancel, Back or Escape discard unsaved edits when the page is reopened. Installation, login and game execution disable editing and Save, while return and saved-instance game/mods/logs shortcuts remain available. Invalid values or inaccessible paths show recovery guidance; rejected saves preserve the previous preferences. Shortcuts use the saved directory, not draft paths.

## Brand Commitments

Eternal name. Owner selected a dark futuristic interface with violet details and direct C++ implementation with visual verification. New logo requested.

## Product Principles

- Starting the game is the primary task.
- Installation and connection status must reflect actual service results.
- Preserve native text editing and window behavior.
- Keep all existing services and workflows functional.
