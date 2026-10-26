# Changes

## 11.24.1 beta — Installer B

- Small installer with an external payload ZIP, bound by size and SHA-256 and read
  through the same write-locked stream. Missing/changed/truncated packages are refused.
- Embedded help/licenses and uninstall/recovery without the original sidecar ZIP.
- Full ZIP distribution and clearer ES/EN/pt-BR installation/update/removal guidance.
- Publishes the exact reviewed/tested B ZIP and unchanged 11.24.0 rendering payload.
- Documents remaining dated antivirus detections, hashes and real-game F8/F9 tests.

## 11.24.0 beta

- Adds analytical AMD FSR Frame Generation. F8 toggles DLSS/DLAA; F9 toggles FG.
- Includes signed AMD dependencies; no OptiScaler, NVIDIA FG or NVIDIA runtime DLL.
- See README and SECURITY for requirements, testing limits and antivirus reports.

## 11.23.4 beta

- Installer UI, messages and logs in Spanish, English and Brazilian Portuguese.
- Automatic initial language and a manual language selector.
- Localized help and mod terms; original NVIDIA and third-party licenses remain unchanged.
- Language switching preserves the selected folder and clears legal consent.
- Adds 12 language regressions. Retains automatic FSR configuration and the unchanged 11.23.2 runtime.

## 11.23.3 beta

- The installer turns off Grounded's FSR setting automatically on install/update.
- Uninstall restores the previous FSR selection while preserving later player edits.
- Adds 16 FSR configuration and recovery regressions.
- The DLSS runtime is the unchanged 11.23.2 binary.

## 11.23.2 beta

- Reverts repeated DLSS history resets introduced in 11.23.1 for unclassified temporal pipelines.
- Restores the prior fixed frame-time input while keeping the other audit fixes.

## 11.23.1 beta

- Audit fixes for startup, process-lifetime hook state, native pending packets, bounded logging, local DLL copying and NGX module identity.
- Ultra Performance uses 33.32% to match NGX dimensions without full-image resampling at the tested outputs.
- Installer recovery and INI encoding preservation improvements.

## Distribution status

Beta with unresolved publication questions documented in LEGAL.md and publication/clearance.json. The author's request for public distribution does not mark those questions resolved. No NVIDIA DLL or SDK headers are included. Original project code is MIT; third-party terms remain separate.
