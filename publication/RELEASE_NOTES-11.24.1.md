# Grounded DLSS 11.24.1 Beta · Installer B

[![English](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/en-selected.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/releases/tag/v11.24.1-beta) [![Español](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/es.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/publication/RELEASE_NOTES-11.24.1.es.md) [![Português (Brasil)](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/pt-BR.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/publication/RELEASE_NOTES-11.24.1.pt-BR.md)

A smaller installer with a separately verified payload. The 11.24.0 rendering runtime is unchanged.

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
> [Security details and VirusTotal reports](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.md).

## Install

1. Close Grounded normally.
2. Download **GroundedDLSS-11.24.1-beta.zip** and extract **all** files into a folder.
3. Run `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Keep **GroundedDLSS-payload.zip beside the EXE**, without extracting the inner ZIP.
4. Choose your language and the Grounded folder containing `Grounded.exe`. Read the terms and licenses; if you agree, choose **Install / update**.
5. Launch Grounded with DirectX 12. Start with **67%** resolution scale for Quality.

## Uninstall

Close Grounded and choose **Uninstall** in the same installer, or run `Desinstalar Grounded DLSS.exe` in `Maine/Binaries/Win64/GroundedDLSS-install`. The external ZIP is not needed for uninstall, recovery or help. Keep the complete package for future installation or updates.

## What changed

- Small installer with an external payload ZIP, checked by size and SHA-256 before use.
- Embedded help and licenses; uninstall and recovery work without the original ZIP.
- The exact B package tested in the game and on VirusTotal is preserved. Only the outer download name changed.

**11.24.1 updates the installer and distribution; rendering remains 11.24.0.** The download contains the exact tested B files. The inner EXE intentionally retains its tested filename and version.

Eight B regression suites, production-guard fixture integration and short real-game F8/F9 checks passed. These checks are not exhaustive compatibility testing.

[Full installation and update guide](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/INSTALLER_B.md) · [Security details and VirusTotal reports](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.md).

## Files

[Source code](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/GroundedDLSS-11.24.1-beta-Source.zip) · [Download checksums](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/SHA256SUMS.txt).

Own code is MIT. NVIDIA and AMD components retain their separate terms. This unofficial beta is published at the author’s request; the unresolved questions in LEGAL.md are not declared cleared. Review logs before sharing them, as they can contain personal paths.
