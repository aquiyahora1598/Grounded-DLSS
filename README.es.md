# Grounded DLSS + FSR Frame Generation

[![English](assets/languages/en.svg)](README.md) [![Español](assets/languages/es-selected.svg)](README.es.md) [![Português (Brasil)](assets/languages/pt-BR.svg)](README.pt-BR.md)

**11.24.1 Beta**

Integración no oficial de DLSS/DLAA y AMD FSR Frame Generation para Grounded. Compatible con **Steam 1.4.7.4815**, Windows 10/11 x64, DirectX 12, NVIDIA RTX y salida SDR.

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
> [Detalles de seguridad e informes de VirusTotal](SECURITY.es.md).

## Instalar

1. Cerrá Grounded normalmente.
2. Descargá **GroundedDLSS-11.24.1-beta.zip** y extraé **todos** los archivos en una carpeta.
3. Abrí `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Dejá **GroundedDLSS-payload.zip junto al EXE**, sin extraer el ZIP interno.
4. Elegí tu idioma y la carpeta Grounded que contiene `Grounded.exe`. Leé los términos y licencias; si aceptás, elegí **Instalar / actualizar**.
5. Iniciá Grounded con DirectX 12. Para empezar, usá escala **67 %** para Calidad.

[Guía completa de instalación y actualización](INSTALLER_B.es.md).

## Desinstalar

Cerrá Grounded y elegí **Desinstalar** en el mismo instalador, o abrí `Desinstalar Grounded DLSS.exe` en `Maine/Binaries/Win64/GroundedDLSS-install`. Para desinstalar, recuperar o consultar la ayuda no hace falta el ZIP externo. Conservá el paquete completo para instalar o actualizar después.

## Escala de resolución

| % | Modo DLSS |
| --- | --- |
| 100 | DLAA |
| 67 | Calidad |
| 58 | Equilibrado |
| 50 | Rendimiento |
| 33 | Ultra Rendimiento |

Se elige el modo compatible más cercano. La nitidez está desactivada. Ultra Rendimiento puede perder mucho detalle.

## DLSS y componentes incluidos

Incluye los DLL AMD originales firmados de Frame Generation y su licencia. **No incluye ni descarga DLL NVIDIA.** Al iniciar, el mod selecciona el DLSS de producción firmado más nuevo y compatible que encuentre entre los componentes locales de NVIDIA. Necesita esa fuente local; una instalación limpia del controlador puede no incluirla. No modifica los perfiles ni la caché original de NVIDIA. No incluye OptiScaler ni Frame Generation de NVIDIA.

**11.24.1 actualiza el instalador y la distribución; el renderizado sigue en 11.24.0.** La descarga conserva los archivos exactos de B que se probaron. El EXE interno mantiene su nombre y versión para identificar esa muestra.

## Pruebas y límites

Se hicieron pruebas breves dentro del juego en una RTX 4070 Laptop a 1080p y 1440p. B pasó la activación de DLSS/FG y los cambios F8/F9. No es una garantía de compatibilidad o rendimiento universal. HDR no está validado; puede haber artefactos breves al entrar en pausa. Frame Generation no duplica la respuesta de los controles.

[Alcance de la validación](docs/INSTALLER_B_VALIDATION.md).

## Compilar desde el código fuente

Requiere Python 3.10+, MSVC C++ x64, Windows SDK y .NET Framework 4.x. Leé la licencia NVIDIA incluida antes de aceptar sus términos.

```text
python tools/fetch_sdk.py --accept-nvidia-sdk-license
python tools/fetch_framegen_sdks.py amd
python tools/build.py loader
python tools/build.py dlss-fsr-framegen
python tools/build_release.py
python tools/build_source_release.py
```

## Licencias y avisos

El código propio usa MIT. Los componentes NVIDIA y AMD conservan sus condiciones separadas. Esta beta no oficial se publica a pedido del autor; los pendientes de LEGAL.md no se declaran resueltos. Revisá los registros antes de compartirlos, porque pueden contener rutas personales.

[MIT](LICENSE) · [AMD](licenses/AMD-FidelityFX-SDK-LICENSE.txt) · [NVIDIA](licenses/NVIDIA-RTX-SDK-LICENSE.txt) · [LEGAL.md](LEGAL.md).

<details>
<summary>Avisos legales originales en inglés</summary>

This software contains source code provided by NVIDIA Corporation.

Grounded DLSS isn't endorsed by Obsidian Entertainment, Inc. and doesn't reflect
the views or opinions of Obsidian Entertainment, Inc. or anyone officially involved
with making Grounded. Grounded and Obsidian Entertainment and all related logos
are trademarks or registered trademarks of Obsidian Entertainment, Inc.

</details>
