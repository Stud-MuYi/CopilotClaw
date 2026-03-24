# CopilotClaw

[中文说明](README.zh-CN.md)

CopilotClaw is a tiny Windows 11 x86_64 background application for one-key OpenClaw gateway toggling.

License: [MIT](LICENSE)
Trademark and icon notice: [NOTICE.md](NOTICE.md)

It is designed for Microsoft PowerToys keyboard remapping:
- Map the Copilot key to launch this executable.
- Press once to toggle OpenClaw on or off.
- The app shows no main window, no console window, no tray icon, and exits immediately after the toggle attempt finishes.
- When the app turns the gateway on, it automatically opens the local control page.
- This is an independent, unofficial project by `Stud-MuYi`.

## Behavior

On each launch the app:
- silently runs `openclaw gateway status --no-color`
- probes `127.0.0.1:18789`
- if OpenClaw appears to be running, silently runs `openclaw gateway stop`
- otherwise, silently runs `openclaw gateway start`
- opens `http://127.0.0.1:18789/` after a successful start
- exits as soon as the state transition completes or times out

To avoid overlapping toggles, only one instance is allowed to run at a time. If another invocation starts while one is already running, the later one exits immediately.

## Requirements

- Windows 11 x86_64
- Visual Studio 2022 with MSVC and CMake 3.25 or newer
- OpenClaw installed and callable from the system `PATH` as `openclaw`
- On MSVC builds, the project uses `/std:c++latest` because current CMake/MSVC cloud runners do not expose `CXX26` as a selectable dialect yet.

Verify the CLI first in PowerShell:

```powershell
openclaw gateway status --no-color
openclaw gateway start
openclaw gateway stop
```

## Build

In Windows PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The output executable will be:

```text
build\Release\CopilotClaw.exe
```

The executable embeds the project icon defined by:
- `assets/CopilotClaw.svg`
- `assets/CopilotClaw.ico`
- `app.rc`

Important:
- Only the repository's original code is open source under MIT.
- The current icon files are Microsoft-derived brand assets and are not covered by the MIT License.
- Microsoft-related files in this repository are included only for study, local reference, or compatibility testing.
- If you plan to publish binaries or redistribute the project publicly, review [NOTICE.md](NOTICE.md) and [NOTICE.zh-CN.md](NOTICE.zh-CN.md) if needed, then replace the Microsoft-derived files first.
- For lower legal risk, you should also consider renaming the app and repository before public release.

## Use With PowerToys

1. Install and open Microsoft PowerToys.
2. Open `Keyboard Manager`.
3. Remap the Copilot key.
4. Set the target action to launch `CopilotClaw.exe`.
5. Save the remap.

After that, pressing the remapped key will silently toggle OpenClaw, and successful gateway starts will open the local control page automatically.

## GitHub Actions

The workflow file is `.github/workflows/build-windows.yml`.

It runs when:
- you push to `main`
- you open a pull request
- you trigger it manually from the `Actions` tab

After the workflow finishes:
- open the workflow run in GitHub
- download the `CopilotClaw-windows-x64` artifact
- extract `CopilotClaw.exe`

## GitHub

If you publish this project under your account, the repository URL will usually be:

```text
https://github.com/Stud-MuYi/CopilotClaw
```

## Project Layout

- `main.cpp`: single-process silent toggle logic
- `CMakeLists.txt`: Windows 11 x86_64 build configuration using the latest available compiler mode
- `assets/CopilotClaw.svg`: Microsoft Copilot-based SVG icon
- `assets/CopilotClaw.ico`: embedded Windows icon built from the SVG
- `app.rc`: Windows icon resource script
- `.github/workflows/build-windows.yml`: GitHub Actions Windows build workflow
- `NOTICE.md`: English trademark and icon redistribution notice
- `NOTICE.zh-CN.md`: Chinese trademark and icon redistribution notice
