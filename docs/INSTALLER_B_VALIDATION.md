# Installer B validation — 11.24.1 packaging release

The published full ZIP has the exact SHA-256 of B analyzed on 24 October 2026.
No game rendering code or binary changed. No NVIDIA DLL is included.
The original signed AMD dependencies and all 15 installed payload files are unchanged.

- Eight B installer suites passed: install/update/uninstall, preservation of user
  configuration, rollback/recovery, FSR menu handling, languages/consent, local
  NVIDIA runtime discovery, AMD FG package handling and external payload verification.
- Payload tests reject missing, changed and truncated archives, verify the stream
  write lock, and verify offline licenses and uninstall without the sidecar ZIP.
- An integration test used production guards, the compatible game executable and
  signed local DLSS 310.9.16.0 in a private fixture; install/update/offline removal
  restored both configuration files exactly.
- Real Grounded Steam 1.4.7.4815 / DirectX 12 tests on 24–25 October used an
  RTX 4070 Laptop GPU. B passed all five states: DLSS+FG on, F9 off/on, F8 off/on.
  FG dispatch stopped while disabled and resumed afterward. Measured callback errors: zero.
- B used 2560×1440 Quality. A and C were also tested; the overall sessions included
  1080p and 1440p. The prior installation, settings and save files were restored
  and verified after the tests. No private saves, captures or personal logs are published.

These are short functional tests on one PC, not exhaustive compatibility/stability
tests or a controlled performance comparison. DXGI counters do not measure physical
scanout or input latency. Dated antivirus findings remain in SECURITY.md.
The publication gates in publication/clearance.json still record unresolved legal
questions; public distribution at the author's request is not third-party clearance.
