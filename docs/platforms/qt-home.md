# Qt Home

The Qt Home app lives in `clients/Qt/oxrsys-home`. It is Linux-first, with active Windows support for runtime install, app launching, and optional global OpenXR registration.

Current responsibilities:

- launch compatible apps with `XR_RUNTIME_JSON` and capture stdout/stderr logs
- persist manually added launcher apps under the platform config directory
- scan and accept dropped Linux `.desktop` files, executables, macOS `.app` bundles, and Windows
  `.exe`/`.bat`/`.cmd`/`.lnk` launchers for Godot/Unity candidates
- create terminal launch scripts for app cards on macOS and Linux without changing the runtime
  manifest selection model
- edit the shared runtime TOML keys for streaming, logging, video codec, encoder preset,
  transport, refresh rate, foveation, upscaling, ABR dynamic resolution, mixed reality, occlusion,
  spatial toggles, and reserved headset-audio configuration
- detect adb devices and configure Quest USB reverse mappings on ports `9944`, `9945`, `9946`, and `9948`
- report macOS WiFi readiness through `networksetup`; Linux keeps the lightweight transport message
- run WiFi readiness, ADB status, device listing, reverse mapping reads, and reverse mapping
  configuration on a serialized worker thread so slow `adb` or `networksetup` calls do not block
  the UI
- show runtime activity and streaming stats from `runtime_status.json`
- install and register the user OpenXR runtime on Linux through `${XDG_CONFIG_HOME:-~/.config}/openxr/1/active_runtime.json`
- install the Windows runtime under `%LOCALAPPDATA%\OXRSys\runtime\current`, launch apps with
  `XR_RUNTIME_JSON`, copy required runtime companion DLLs, and optionally register
  `HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` through UAC
- launch apps with either the installed runtime manifest or the manually selected manifest
- open the shared Qt simulator widget from the Developer tab in a reusable `1280x720` window,
  including H.265/H.264 video preview when FFmpeg is available and mouse-driven synthetic head
  tracking

Build with the top-level CMake project:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOXRSYS_BUILD_QT_FRONTENDS=ON
cmake --build build --target oxrsys-home
ctest --test-dir build --output-on-failure
```

On Linux, `OXRSYS_BUILD_QT_FRONTENDS=AUTO` enables the Qt apps when Qt6 Core/Widgets/Network are found.
On macOS, the Qt finder also checks Qt Online Installer prefixes under `~/Qt/<version>/<kit>`,
for example `~/Qt/6.10.2/macos`, in addition to Homebrew, MacPorts, `QTDIR`, and `Qt6_DIR`.
On Windows, `scripts/windows_configure.ps1` defaults Qt frontends to `AUTO`, which keeps runtime
builds working without Qt installed; pass `-QtFrontends ON` after installing Qt 6 to build Qt Home
and the Qt simulator. Qt Online Installer kits under `C:\Qt\<version>\<kit>` are detected
automatically; otherwise set `CMAKE_PREFIX_PATH` to the kit root or `Qt6_DIR` to
`<kit>\lib\cmake\Qt6`.

The simulator shared target also has internal tests for UDP frame assembly, FEC recovery, partial
frame drops, ignored render-pose packets, tracking flags, and shift-modified controller movement.

The Settings tab owns runtime selection, registration, install preference, and ADB setup. On Linux,
`Update Registration` writes `${XDG_CONFIG_HOME:-~/.config}/openxr/1/active_runtime.json` to the
selected manifest. On Windows, `Update Registration` writes
`HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` through an explicit UAC prompt, stores the previous
runtime in `QSettings`, and restores it on unregister only if ActiveRuntime still points to the
OXRSys manifest. The `Use installed runtime for launches` checkbox controls whether Home-launched
apps prefer the installed copy under the platform data directory or use the manifest selected in the
registration field.

The Streaming tab autosaves TOML edits after a short debounce. `Default` restores the structured
streaming, general runtime-enabled, and logging keys to their built-in defaults and writes the file
immediately. `Reveal Runtime Logs` opens the platform state directory that contains
`oxrsys-runtime.log`, `oxrsys-headset.log`, and `runtime_status.json` when those files exist.
The bitrate slider uses the shared runtime range, `1` to `200` Mbps. Runtime status reports
`configured_bitrate_mbps` for the Home/server value and `max_bitrate_mbps` for the effective cap after
client negotiation. The Qt simulator sends `ClientConnect.maxBitrateMbps = 0`, so it does not add a
client-side bitrate cap and both values should match when the simulator connects.

The Streaming tab also exposes the shared refresh-rate choices `60`, `72`, `80`, `90`, and `120`
Hz, video codec choices `h265`, `h264`, and `auto`, encoder presets `speed`, `balanced`, and
`quality`, negotiated `encoder_10bit` HEVC Main10 streaming for capable H.265 clients, and
server-side foveated encoding presets `off`, `light`, `medium`, and `high`. It also
exposes `abr_mode` with `off`, `bitrate`, and `full`, `dynamic_resolution_min_scale`, passthrough
enablement, occlusion mode, and the `[spatial]` feature toggles. The runtime stats view shows when a
requested foveated encoding preset is inactive, including the `resolution_scale < 1` guard.
The runtime stats view separates the global passthrough setting from headset support: `unsupported`
means the selected client did not advertise `CLIENT_CAPABILITY_MIXED_REALITY_PASSTHROUGH` after
querying its local OpenXR runtime.
In `abr_mode = "full"`, compatible headset clients can receive `StreamConfigUpdate/Ack` messages on
the reliable USB TCP control path so the runtime changes encoded stream resolution without resizing
the OpenXR application's swapchains. WiFi clients remain bitrate-only for live changes in this
version. The spatial controls and `9948` reverse mapping remain reserved scaffolding until a backend
is attached.
The Headset Client section owns headset-side options:
`auto` leaves Quest/PICO `XR_FB_foveation` unmanaged by Home, while `off`, `light`, `medium`,
and `high` explicitly override the viewer swapchains. Quest shader upscaling and the reserved
headset-audio toggle live there as well. `client_reprojection` controls Quest/PICO missing-frame
smoothing with `off`, `pose`, and `pose_warp`. Audio is not reported active by the runtime until a
real capture/playback stream is implemented.

Transport readiness work is asynchronous. Qt Home shows checking/configuring status while the
worker is running, ignores stale results after ADB mode, ADB path, selected serial, or transport
changes, and keeps the previous main transport selection if USB validation fails. After a successful
Quest USB reverse configuration, Qt Home keeps that verified serial and port set if a later ADB poll
can no longer see the headset, which can happen on some Windows/Quest setups when the VR app enters
immersive mode. ADB must still see the device again before Home can prove that mappings changed or
apply a fresh reverse setup.

Platform behavior:

- Linux is the complete target for OpenXR registration, `.desktop` launch, USB ADB, config, state, and launcher persistence.
- macOS can build and use the Qt launcher with `.app` bundles and executables, drag-and-drop
  additions, app-card terminal launch via generated `.command` files, macOS WiFi readiness checks,
  and improved ADB guidance. Runtime registration stays owned by the SwiftUI Home app for now.
- Linux app-card terminal launch tries `x-terminal-emulator`, `gnome-terminal`, `konsole`, then
  `xterm`. No new terminal dependency is required.
- Windows scans Start Menu `.lnk` files via the Shell COM API, accepts manual `.exe`, `.bat`,
  `.cmd`, and `.lnk` additions, installs the bundled runtime locally, and launches apps with
  `XR_RUNTIME_JSON` without admin. App-card terminal actions remain hidden on Windows.

Windows notes:

- Build with `scripts/windows_configure.ps1` to let vcpkg install FFmpeg automatically, or pass
  `-FFmpegRoot <path>` when using a local FFmpeg development package. Vcpkg builds use static
  FFmpeg library linkage by default (`x64-windows-static-md` or `arm64-windows-static-md`) so the
  runtime DLL does not require FFmpeg companion DLLs. Pass `-DynamicFFmpeg` only when dynamic vcpkg
  FFmpeg DLLs are desired. The runtime requires FFmpeg headers/import libraries; the Qt simulator
  uses the same root opportunistically.
  For `windows-arm64`, use ARM64 FFmpeg and Qt packages and run from an ARM64-capable Visual Studio
  developer environment or matching IDE preset activation.
- `scripts/windows_configure.ps1` can locate Visual Studio CMake/Ninja/MSVC/vcpkg tools and configure
  the Windows build when the current shell has not picked up fresh `PATH` changes.
- Windows builds run `windeployqt` after linking `oxrsys-home.exe` and `oxrsys-simulator.exe` so
  the required Qt DLLs and plugins are copied into the executable directories.
- `oxrsys-home.exe` and `oxrsys-simulator.exe` embed icons generated from the corresponding Xcode
  app icon assets. They are built as GUI-subsystem apps, so launching Qt Home from Explorer does not
  open a console window.
- Global registration is machine-wide HKLM state. Qt Home uses `ShellExecuteEx(..., "runas")` for
  register/unregister and waits for the elevated helper to finish before refreshing the status.
- Windows runtime launches can use Vulkan, D3D11, or D3D12 OpenXR applications. Video streaming is
  currently limited to common RGBA/BGRA 8-bit Vulkan or DXGI color swapchain formats; unsupported
  formats are logged and dropped without changing the OpenXR wire protocol.
- Qt Home's Windows install action copies `oxrsys-runtime.dll`, `oxrsys-runtime.toml`, and any
  sibling runtime DLLs into `%LOCALAPPDATA%\OXRSys\runtime\current`. Static FFmpeg builds normally
  have no FFmpeg companion DLLs; dynamic builds still report an update when any bundled companion
  DLL is missing or differs from the installed copy.

Selecting USB in the header validates the selected or single authorized Quest device and configures
missing reverse mappings automatically before persisting `streaming.transport = "usb_adb"`. If
multiple authorized devices are present, pick one in the Settings ADB panel first.

In `Internal` ADB mode, Qt Home first talks directly to a running local ADB server on
`127.0.0.1:5037`. This avoids launching an `adb` executable for reverse setup when another Android
tool has already started the server. If no server responds, Qt Home falls back to SDK, Homebrew, and
`PATH` executable discovery. The SDK-free native USB ADB backend currently belongs to the macOS
SwiftUI Home app; Qt Home keeps the portable server/executable path. In `Custom` mode, Qt Home tries
only the configured executable path; `Auto Detect` pre-fills that path from the detected external
`adb` executable when one is available.

If USB mode reports missing ADB in Qt Home on macOS and no ADB server is already running, install
the optional `adb-enhanced` fallback with Homebrew:

```bash
brew install adb-enhanced
```

The error message lists the candidate paths that were checked. The Settings ADB panel has `Browse`
and `Auto Detect` actions. A selected custom path is stored in Qt Home `QSettings`, is not shared
with the SwiftUI Home, and must be executable and pass `adb version`. If it becomes empty or invalid,
Qt Home reports that selected path and does not silently fall back to Internal mode.
