# Seguridad y estado antivirus

[![English](assets/languages/en.svg)](SECURITY.md) [![Español](assets/languages/es-selected.svg)](SECURITY.es.md) [![Português (Brasil)](assets/languages/pt-BR.svg)](SECURITY.pt-BR.md)

**11.24.1 Beta · Installer B**

Estas observaciones se registraron el **24 de octubre de 2026** y no se actualizaron con este cambio de documentación. Los resultados pueden cambiar. La release conserva los archivos exactos de B que se probaron.

**Persisten detecciones antivirus.** El 24 de octubre de 2026, el EXE del instalador tenía **5/70 detecciones** y el ZIP completo **2/65**, con distinta cobertura. Son resultados de esa fecha, no una certificación. Si se bloquea, mantenelo bloqueado; no desactives la protección ni agregues exclusiones.

| Muestra | SHA-256 | Resultado observado | Microsoft en ese informe |
| --- | --- | --- | --- |
| ZIP completo de descarga | `86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4` | 2/65 | Undetected |
| EXE del instalador dentro del ZIP | `c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca` | 5/70 | Undetected |
| ZIP interno del contenido | `d965884af71330028edfd17fdd4883e4fe678aa2621601aa81194ae674ba5e04` | Sin análisis separado declarado | — |

[VirusTotal · ZIP](https://www.virustotal.com/gui/file/86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4/detection) · [VirusTotal · EXE](https://www.virustotal.com/gui/file/c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca/detection).

**El resultado del ZIP no certifica su contenido.** Los dos contadores tienen distinta cobertura. Bkav y Elastic detectaban el ZIP completo; el EXE también tenía detecciones de CrowdStrike, McAfee y SecureAge. Que Microsoft no lo detecte en VirusTotal no garantiza que Defender no lo bloquee en otra PC.

Defender local no encontró amenazas con las firmas 1.461.188.0. Ese resultado, las comprobaciones de integridad y las pruebas del juego no demuestran que las otras detecciones sean falsos positivos. El instalador, cargador y ASI no tienen firma digital; los dos DLL AMD incluidos conservan sus firmas originales.

El último estado registrado de la consulta original a Microsoft era **In progress** el 24 de octubre. No se declara un dictamen posterior ni una aprobación general de los fabricantes.

[Alcance de la validación](docs/INSTALLER_B_VALIDATION.md) · [Informe de seguridad anterior](https://github.com/aquiyahora1598/Grounded-DLSS/blob/8fb24196042063e09521bb41744ba45938a1acf7/SECURITY.md).
