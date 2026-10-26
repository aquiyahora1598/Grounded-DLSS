# Grounded DLSS + FSR Frame Generation

[![English](assets/languages/en-selected.svg)](README.md) [![Español](assets/languages/es.svg)](README.es.md) [![Português (Brasil)](assets/languages/pt-BR.svg)](README.pt-BR.md)

**11.24.1 Beta**

An unofficial DLSS/DLAA and AMD FSR Frame Generation integration for Grounded. Supports **Steam 1.4.7.4815**, Windows 10/11 x64, DirectX 12, NVIDIA RTX and SDR output.

**[Download 11.24.1 Beta](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/GroundedDLSS-11.24.1-beta.zip)**

## Controls

| | |
| --- | --- |
| **F8 · DLSS / DLAA** | Enable or disable DLSS / DLAA |
| **F9 · FSR Frame Generation** | Enable or disable FSR Frame Generation |

Both start enabled. Use F9 to compare Frame Generation alone. Turning DLSS off with F8 suspends Frame Generation; it resumes when DLSS is enabled again if F9 remained on.

**Keep the in-game AMD FSR upscaler OFF.** The installer switches it off automatically. Frame Generation is controlled separately with F9.

> [!WARNING]
> **Antivirus detections remain.** On 24 October 2026, the installer EXE had **5/70 detections** and the full ZIP **2/65**, with different coverage. These are dated results, not security clearance. If blocked, keep the file blocked; do not disable protection or add exclusions.
>
> [Security details and VirusTotal reports](SECURITY.md).

## Install

1. Close Grounded normally.
2. Download **GroundedDLSS-11.24.1-beta.zip** and extract **all** files into a folder.
3. Run `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Keep **GroundedDLSS-payload.zip beside the EXE**, without extracting the inner ZIP.
4. Choose your language and the Grounded folder containing `Grounded.exe`. Read the terms and licenses; if you agree, choose **Install / update**.
5. Launch Grounded with DirectX 12. Start with **67%** resolution scale for Quality.

[Full installation and update guide](INSTALLER_B.md).

## Uninstall

Close Grounded and choose **Uninstall** in the same installer, or run `Desinstalar Grounded DLSS.exe` in `Maine/Binaries/Win64/GroundedDLSS-install`. The external ZIP is not needed for uninstall, recovery or help. Keep the complete package for future installation or updates.

## Resolution scale

| % | DLSS mode |
| --- | --- |
| 100 | DLAA |
| 67 | Quality |
| 58 | Balanced |
| 50 | Performance |
| 33 | Ultra Performance |

The closest supported mode is selected. Sharpening is disabled. Ultra Performance can lose substantial detail.

## DLSS and included components

Includes the original signed AMD Frame Generation DLLs and their license. **No NVIDIA DLL is bundled or downloaded.** At startup the mod selects the newest compatible, signed production DLSS available in local NVIDIA components. It needs that local source; a clean driver installation may not contain it. It does not change NVIDIA profiles or the original cache. No OptiScaler or NVIDIA Frame Generation is included.

**11.24.1 updates the installer and distribution; rendering remains 11.24.0.** The download contains the exact tested B files. The inner EXE intentionally retains its tested filename and version.

## Testing and limits

Short real-game tests on an RTX 4070 Laptop covered 1080p and 1440p; B passed DLSS/FG activation and F8/F9 round trips. This is not universal compatibility or a performance guarantee. HDR is unvalidated; brief pause-transition artifacts may occur. Frame Generation does not double input responsiveness.

[Validation scope](docs/INSTALLER_B_VALIDATION.md).

## Build from source

Requires Python 3.10+, MSVC C++ x64, Windows SDK and .NET Framework 4.x. Read the included NVIDIA license before accepting its terms.

```text
python tools/fetch_sdk.py --accept-nvidia-sdk-license
python tools/fetch_framegen_sdks.py amd
python tools/build.py loader
python tools/build.py dlss-fsr-framegen
python tools/build_release.py
python tools/build_source_release.py
```

## Licenses and notices

Own code is MIT. NVIDIA and AMD components retain their separate terms. This unofficial beta is published at the author’s request; the unresolved questions in LEGAL.md are not declared cleared. Review logs before sharing them, as they can contain personal paths.

[MIT](LICENSE) · [AMD](licenses/AMD-FidelityFX-SDK-LICENSE.txt) · [NVIDIA](licenses/NVIDIA-RTX-SDK-LICENSE.txt) · [LEGAL.md](LEGAL.md).

<details>
<summary>Original legal notices</summary>

This software contains source code provided by NVIDIA Corporation.

Grounded DLSS isn't endorsed by Obsidian Entertainment, Inc. and doesn't reflect
the views or opinions of Obsidian Entertainment, Inc. or anyone officially involved
with making Grounded. Grounded and Obsidian Entertainment and all related logos
are trademarks or registered trademarks of Obsidian Entertainment, Inc.

</details>
