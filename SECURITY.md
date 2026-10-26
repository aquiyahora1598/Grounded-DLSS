# Security and antivirus status

[![English](assets/languages/en-selected.svg)](SECURITY.md) [![Español](assets/languages/es.svg)](SECURITY.es.md) [![Português (Brasil)](assets/languages/pt-BR.svg)](SECURITY.pt-BR.md)

**11.24.1 Beta · Installer B**

These observations were recorded on **24 October 2026** and have not been refreshed for this documentation update. Results can change. The release publishes the exact tested B files.

**Antivirus detections remain.** On 24 October 2026, the installer EXE had **5/70 detections** and the full ZIP **2/65**, with different coverage. These are dated results, not security clearance. If blocked, keep the file blocked; do not disable protection or add exclusions.

| Sample | SHA-256 | Observed result | Microsoft in that report |
| --- | --- | --- | --- |
| Full download ZIP | `86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4` | 2/65 | Undetected |
| Installer EXE inside ZIP | `c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca` | 5/70 | Undetected |
| Inner payload ZIP | `d965884af71330028edfd17fdd4883e4fe678aa2621601aa81194ae674ba5e04` | No separate scan claimed | — |

[VirusTotal · ZIP](https://www.virustotal.com/gui/file/86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4/detection) · [VirusTotal · EXE](https://www.virustotal.com/gui/file/c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca/detection).

**The ZIP result does not clear its contents.** The two counts have different coverage. Bkav and Elastic flagged the full ZIP; the EXE also had CrowdStrike, McAfee and SecureAge detections. Microsoft being undetected in VirusTotal does not guarantee no Defender block on another PC.

Local Defender found no threats with signatures 1.461.188.0. That result, integrity checks and game tests do not establish that the remaining detections are false positives. The installer, loader and ASI are unsigned; the two included AMD DLLs retain their original signatures.

The original Microsoft submission’s last recorded status was **In progress** on 24 October. No later analyst verdict or general vendor clearance is claimed.

[Validation scope](docs/INSTALLER_B_VALIDATION.md) · [Previous security report](https://github.com/aquiyahora1598/Grounded-DLSS/blob/8fb24196042063e09521bb41744ba45938a1acf7/SECURITY.md).
