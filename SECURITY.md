# Antivirus status for 11.24.1 Beta / Installer B

This packaging update publishes the **exact tested B binaries**. It is not a
claim that the antivirus issue has been resolved. Observations below were recorded
on **24 October 2026**; they have not been refreshed for this release and can change.

| Sample | SHA-256 | Observed result | Microsoft in that report |
| --- | --- | --- | --- |
| Full download ZIP | `86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4` | 2/65 | Undetected |
| EXE inside ZIP | `c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca` | 5/70 | Undetected |
| Inner payload ZIP | `d965884af71330028edfd17fdd4883e4fe678aa2621601aa81194ae674ba5e04` | No separate VT result claimed | No separate result claimed |

[Full ZIP report](https://www.virustotal.com/gui/file/86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4/detection) ·
[EXE report](https://www.virustotal.com/gui/file/c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca/detection).

The two counts have different coverage. **2/65 does not cancel the five alerts on
the enclosed EXE.** Bkav and Elastic flagged the full ZIP; the EXE also had
CrowdStrike, McAfee and SecureAge detections. A clean Microsoft result in VirusTotal
does not guarantee no Defender block elsewhere. Local Defender found no threats
with signatures 1.461.188.0, but that is not independent clearance or a safety certificate.

**ES:** Persisten detecciones; los posibles falsos positivos no están confirmados.
Si se bloquea, mantenelo bloqueado y reportá versión, hash y nombre de detección.
No desactives la protección ni añadas exclusiones.
**EN:** Detections remain; possible false positives are unconfirmed. Keep blocked
files blocked. Do not disable protection or add exclusions.
**PT-BR:** As detecções continuam; possíveis falsos positivos não foram confirmados.
Mantenha arquivos bloqueados. Não desative a proteção nem adicione exclusões.

The original Microsoft submission's last recorded state was In progress on
24 October. No later analyst verdict or vendor clearance is claimed here.
See [functional validation](docs/INSTALLER_B_VALIDATION.md) for what was actually tested.
Integrity checks and game tests do not determine whether an antivirus verdict is correct.

---

# Historical report for the previous 11.24.0 installer

# Antivirus / Security / Segurança

## Aviso para 11.24.0 Beta — 23/10/2026

**Se reportó que Microsoft Defender bloquea el instalador como
`Trojan:Win32/Sabsik.FL.A!ml`. El problema sigue en revisión.**

El instalador exacto se envió a VirusTotal. En el análisis del **23 de octubre
de 2026**, **9 de 70 motores lo marcaron como malicioso**. El motor de Microsoft
figuraba como **Undetected** en ese informe; eso no invalida el bloqueo reportado
en otra PC. Los resultados de VirusTotal pueden cambiar.

**Puede haber falsos positivos, pero todavía no están confirmados.** Ni la mayoría
de motores sin detección ni un análisis local limpio certifican que un programa
sea seguro. No presentamos VirusTotal como una certificación de seguridad.

- [Informe público completo de VirusTotal](https://www.virustotal.com/gui/file/88db9f4431bea059adf86b9825abb1c2fde18edf6eb5e1930bd4508198edf485/detection).
- Se solicitó revisión a Microsoft el **23/10/2026**. Último estado verificado:
  **Submitted / Pending**, sin dictamen del analista. No hay confirmación de un
  falso positivo ni de que se haya corregido la detección.
- Si tu antivirus bloquea el archivo, **mantenelo bloqueado y esperá la revisión**.
  No desactives la protección ni añadas exclusiones para instalarlo.

## Archivo al que corresponde el informe

| Dato | Valor |
| --- | --- |
| Versión | 11.24.0 Beta |
| Archivo | `GroundedDLSS-11.24.0-beta-Setup.exe` |
| Tamaño | 31.012.864 bytes |
| SHA-256 | `88db9f4431bea059adf86b9825abb1c2fde18edf6eb5e1930bd4508198edf485` |
| Resultado de VirusTotal observado | 9/70 |
| Fecha del análisis | 23/10/2026, 02:47 (Argentina, UTC−3) |

El informe corresponde únicamente a esos bytes del instalador, no a otras
versiones ni a todos los archivos generados al usar el mod. Para comparar una
descarga, calculá su SHA-256 y contrastalo con el de arriba y `SHA256SUMS.txt` de
la versión. Un hash coincidente demuestra identidad del archivo, no seguridad.

Motores que lo marcaron en ese análisis:

| Motor | Etiqueta |
| --- | --- |
| Arctic Wolf | `Unsafe` |
| Bkav Pro | `W32.Malware.B0E5C6F9` |
| CrowdStrike Falcon | `Win/malicious_confidence_70% (D)` |
| Cynet | `Malicious (score: 100)` |
| Elastic | `Malicious (high Confidence)` |
| McAfee Scanner | `Ti!88DB9F4431BE` |
| SecureAge | `Malicious` |
| Symantec | `ML.Attribute.HighConfidence` |
| Trapmine | `Malicious.high.ml.score` |

## Qué se comprobó localmente

La copia descargada de GitHub coincide con el instalador publicado. Los 15
archivos de su paquete integrado coinciden con el manifiesto de construcción.
Los dos DLL AMD conservan firmas válidas de Advanced Micro Devices. El instalador,
el cargador propio y el ASI propio no tienen firma Authenticode.

Defender local, con motor `1.1.26090.9` y firmas `1.461.158.0`, no detectó amenazas
en el instalador ni en los componentes analizados por separado. Se recompiló el
instalador desde las fuentes públicas: coincide salvo la fecha PE y el identificador
CLR de compilación. Estas comprobaciones de integridad no sustituyen la revisión
de los fabricantes de antivirus ni constituyen un dictamen de inocuidad.

El mod utiliza hooks gráficos y cambios de memoria dentro del proceso de Grounded
para integrar DLSS y Frame Generation. Esos mecanismos están en el código público.
No se ha aislado qué característica concreta originó las detecciones; no afirmamos
que el nombre del aviso, la falta de firma o los hooks prueben un falso positivo.

## English

Microsoft Defender has been reported to block **11.24.0 Beta** as
`Trojan:Win32/Sabsik.FL.A!ml`. The exact installer was submitted to VirusTotal:
**9/70 engines flagged it on October 23, 2026**. Microsoft showed **Undetected**
in that report, which does not invalidate the reported block on another PC.
**False positives are possible but have not been confirmed.** A review was
submitted to Microsoft; the last verified status is **Submitted / Pending**,
without an analyst determination. If your antivirus blocks this file, keep it
blocked and wait for review. Do not disable protection or add exclusions to run it.
The full public report and exact SHA-256 are above. The report is not a safety
certificate and does not cover other versions. Results can change.

## Português (Brasil)

Foi relatado que o Microsoft Defender bloqueia a **11.24.0 Beta** como
`Trojan:Win32/Sabsik.FL.A!ml`. O instalador exato foi enviado ao VirusTotal:
**9/70 mecanismos o sinalizaram em 23/10/2026**. A Microsoft apareceu como
**Undetected** nesse relatório; isso não invalida o bloqueio relatado em outro PC.
**Podem ocorrer falsos positivos, mas ainda não foram confirmados.** Foi solicitada
uma revisão à Microsoft; o último estado verificado é **Submitted / Pending**,
sem parecer do analista. Se o antivírus bloquear o arquivo, mantenha-o bloqueado
e aguarde a revisão. Não desative a proteção nem adicione exclusões para executá-lo.
O relatório público completo e o SHA-256 exato estão acima. O relatório não é um
certificado de segurança e não se aplica a outras versões. Os resultados podem mudar.

## Referencias / References / Referências

- [Qué analiza y comparte VirusTotal](https://docs.virustotal.com/docs/how-it-works).
- [Microsoft: revisión de detecciones para desarrolladores](https://learn.microsoft.com/en-us/defender-xdr/developer-faq).
- [Microsoft: significado de los estados de una solicitud](https://learn.microsoft.com/en-us/defender-xdr/submission-guide).

