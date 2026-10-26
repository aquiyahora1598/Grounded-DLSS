# Full installation and update guide

[![English](assets/languages/en-selected.svg)](INSTALLER_B.md) [![Español](assets/languages/es.svg)](INSTALLER_B.es.md) [![Português (Brasil)](assets/languages/pt-BR.svg)](INSTALLER_B.pt-BR.md)

**11.24.1 Beta · Installer B**

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

## Controls

| | |
| --- | --- |
| **F8 · DLSS / DLAA** | Enable or disable DLSS / DLAA |
| **F9 · FSR Frame Generation** | Enable or disable FSR Frame Generation |

Both start enabled. Use F9 to compare Frame Generation alone. Turning DLSS off with F8 suspends Frame Generation; it resumes when DLSS is enabled again if F9 remained on.

**Keep the in-game AMD FSR upscaler OFF.** The installer switches it off automatically. Frame Generation is controlled separately with F9.

## Update or reinstall

With Grounded closed, extract the complete package and use **Install / update**. Retain both the EXE and its sidecar ZIP. Do not use the old research BAT mode selectors with a new installation. Uninstall preserves unrelated settings and user-modified files.

## Uninstall

Close Grounded and choose **Uninstall** in the same installer, or run `Desinstalar Grounded DLSS.exe` in `Maine/Binaries/Win64/GroundedDLSS-install`. The external ZIP is not needed for uninstall, recovery or help. Keep the complete package for future installation or updates.

## Package identity

**11.24.1 updates the installer and distribution; rendering remains 11.24.0.** The download contains the exact tested B files. The inner EXE intentionally retains its tested filename and version.

A missing, truncated or modified sidecar ZIP is refused. The complete ZIP is checked against a size and SHA-256 bound into the installer, using the same stream locked against writes. Help and licenses are embedded for offline access.

[Build from source](README.md) · [Security details and VirusTotal reports](SECURITY.md).
