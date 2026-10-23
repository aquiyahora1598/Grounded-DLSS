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

