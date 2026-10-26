# Guía completa de instalación y actualización

[![English](assets/languages/en.svg)](INSTALLER_B.md) [![Español](assets/languages/es-selected.svg)](INSTALLER_B.es.md) [![Português (Brasil)](assets/languages/pt-BR.svg)](INSTALLER_B.pt-BR.md)

**11.24.1 Beta · Installer B**

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

## Controles

| | |
| --- | --- |
| **F8 · DLSS / DLAA** | Activar o desactivar DLSS / DLAA |
| **F9 · FSR Frame Generation** | Activar o desactivar FSR Frame Generation |

Ambos inician activados. Usá F9 para comparar solo Frame Generation. Apagar DLSS con F8 suspende Frame Generation; se reanuda al activar DLSS si F9 seguía encendido.

**Dejá APAGADO el escalado FSR de AMD del menú.** El instalador lo apaga automáticamente. Frame Generation se controla por separado con F9.

## Actualizar o reinstalar

Con Grounded cerrado, extraé el paquete completo y usá **Instalar / actualizar**. Conservá el EXE y el ZIP que lo acompaña. No uses los antiguos selectores BAT de investigación sobre una instalación nueva. La desinstalación conserva ajustes ajenos y archivos modificados por el usuario.

## Desinstalar

Cerrá Grounded y elegí **Desinstalar** en el mismo instalador, o abrí `Desinstalar Grounded DLSS.exe` en `Maine/Binaries/Win64/GroundedDLSS-install`. Para desinstalar, recuperar o consultar la ayuda no hace falta el ZIP externo. Conservá el paquete completo para instalar o actualizar después.

## Identidad del paquete

**11.24.1 actualiza el instalador y la distribución; el renderizado sigue en 11.24.0.** La descarga conserva los archivos exactos de B que se probaron. El EXE interno mantiene su nombre y versión para identificar esa muestra.

Se rechaza un ZIP externo ausente, truncado o modificado. Se verifica su tamaño y SHA-256 contra los valores incluidos en el instalador, usando el mismo archivo bloqueado contra escrituras mientras se lee. La ayuda y las licencias están integradas para consultarlas sin el ZIP.

[Compilar desde el código fuente](README.es.md) · [Detalles de seguridad e informes de VirusTotal](SECURITY.es.md).
