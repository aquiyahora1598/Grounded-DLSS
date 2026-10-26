# Grounded DLSS 11.24.1 Beta · Installer B

[![English](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/en.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/releases/tag/v11.24.1-beta) [![Español](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/es-selected.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/publication/RELEASE_NOTES-11.24.1.es.md) [![Português (Brasil)](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/pt-BR.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/publication/RELEASE_NOTES-11.24.1.pt-BR.md)

Un instalador más pequeño con su contenido separado y verificado. El renderizado de 11.24.0 se conserva.

**[Descargar 11.24.1 Beta](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/GroundedDLSS-11.24.1-beta.zip)**

## Controles

| | |
| --- | --- |
| **F8 · DLSS / DLAA** | Activar o desactivar DLSS / DLAA |
| **F9 · FSR Frame Generation** | Activar o desactivar FSR Frame Generation |

Ambos inician activados. Usá F9 para comparar solo Frame Generation. Apagar DLSS con F8 suspende Frame Generation; se reanuda al activar DLSS si F9 seguía encendido.

**Dejá APAGADO el escalado FSR de AMD del menú.** El instalador lo apaga automáticamente. Frame Generation se controla por separado con F9.

> [!WARNING]
> **Persisten detecciones antivirus.** El 24 de octubre de 2026, el EXE del instalador tenía **5/70 detecciones** y el ZIP completo **2/65**, con distinta cobertura. Son resultados de esa fecha, no una certificación. Si se bloquea, mantenelo bloqueado; no desactives la protección ni agregues exclusiones.
>
> [Detalles de seguridad e informes de VirusTotal](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.es.md).

## Instalar

1. Cerrá Grounded normalmente.
2. Descargá **GroundedDLSS-11.24.1-beta.zip** y extraé **todos** los archivos en una carpeta.
3. Abrí `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Dejá **GroundedDLSS-payload.zip junto al EXE**, sin extraer el ZIP interno.
4. Elegí tu idioma y la carpeta Grounded que contiene `Grounded.exe`. Leé los términos y licencias; si aceptás, elegí **Instalar / actualizar**.
5. Iniciá Grounded con DirectX 12. Para empezar, usá escala **67 %** para Calidad.

## Desinstalar

Cerrá Grounded y elegí **Desinstalar** en el mismo instalador, o abrí `Desinstalar Grounded DLSS.exe` en `Maine/Binaries/Win64/GroundedDLSS-install`. Para desinstalar, recuperar o consultar la ayuda no hace falta el ZIP externo. Conservá el paquete completo para instalar o actualizar después.

## Qué cambió

- Instalador pequeño con un ZIP externo, verificado por tamaño y SHA-256 antes de usarlo.
- Ayuda y licencias integradas; desinstalación y recuperación sin el ZIP original.
- Se conserva el paquete B exacto que se probó en el juego y en VirusTotal. Solo cambió el nombre de la descarga externa.

**11.24.1 actualiza el instalador y la distribución; el renderizado sigue en 11.24.0.** La descarga conserva los archivos exactos de B que se probaron. El EXE interno mantiene su nombre y versión para identificar esa muestra.

Pasaron ocho suites de regresión de B, la integración aislada con controles de producción y las pruebas breves de F8/F9 en el juego. No son pruebas exhaustivas de compatibilidad.

[Guía completa de instalación y actualización](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/INSTALLER_B.es.md) · [Detalles de seguridad e informes de VirusTotal](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.es.md).

## Archivos

[Código fuente](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/GroundedDLSS-11.24.1-beta-Source.zip) · [Sumas de verificación](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/SHA256SUMS.txt).

El código propio usa MIT. Los componentes NVIDIA y AMD conservan sus condiciones separadas. Esta beta no oficial se publica a pedido del autor; los pendientes de LEGAL.md no se declaran resueltos. Revisá los registros antes de compartirlos, porque pueden contener rutas personales.
