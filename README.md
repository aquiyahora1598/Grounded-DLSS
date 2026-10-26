# Grounded DLSS + FSR Frame Generation

<!-- antivirus-notice-11.24.1 -->
> [!WARNING]
> **11.24.1 Beta / Installer B — antivirus observations from 24/10/2026**
>
> **ES:** El EXE de B conservaba **5/70 detecciones**. Su ZIP completo mostraba
> **2/65**, con distinta cobertura; ese número no elimina las alertas del EXE interior.
> Microsoft figuraba sin detección en ambas muestras. Los posibles falsos positivos
> siguen sin confirmación general. Si se bloquea, no desactives la protección ni añadas exclusiones.
>
> **EN:** B's EXE still had **5/70 detections**; the full ZIP had **2/65** with
> different coverage. The ZIP result does not clear its contents. Possible false
> positives remain unconfirmed. Do not disable protection or add exclusions.
>
> **PT-BR:** O EXE de B ainda tinha **5/70 detecções**; o ZIP completo tinha
> **2/65**, com cobertura diferente. O resultado do ZIP não libera seu conteúdo.
> Possíveis falsos positivos não foram confirmados. Não desative a proteção nem adicione exclusões.
>
> [EXE report](https://www.virustotal.com/gui/file/c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca/detection) ·
> [Full ZIP report](https://www.virustotal.com/gui/file/86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4/detection) ·
> [Hashes, scope and history](SECURITY.md). These are dated observations, not a safety certificate.
<!-- /antivirus-notice-11.24.1 -->

**11.24.1 Beta · Español / English / Português (Brasil)**

## F8 — DLSS / DLAA ON ↔ OFF

## F9 — FSR FRAME GENERATION ON ↔ OFF

**Español:** F8 activa/desactiva DLSS/DLAA. **F9 activa/desactiva FSR Frame Generation.**
**English:** F8 enables/disables DLSS/DLAA. **F9 enables/disables FSR Frame Generation.**
**Português:** F8 ativa/desativa DLSS/DLAA. **F9 ativa/desativa FSR Frame Generation.**

Ambos inician activados / Both start enabled / Ambos iniciam ativados.
Para comparar solo FG, usá F9 / Use F9 to compare FG alone / Use F9 para comparar somente o FG.

> **FSR de AMD del menú: APAGADO.** Es otro escalador; el instalador lo desactiva.
> Keep the in-game **AMD FSR upscaler OFF**. Deixe o **escalonamento FSR do menu DESATIVADO**.
> Frame Generation se controla aparte con **F9**.

Integración no oficial para Grounded Steam **1.4.7.4815**, Windows 10/11 x64,
DirectX 12, NVIDIA RTX y salida **SDR**. DLSS reconstruye la imagen; AMD FSR 3.1.6
analítico genera los cuadros intermedios con su presentación 3.1.7. Sin OptiScaler
ni NVIDIA FG. Código propio MIT; dependencias bajo sus licencias originales.

## Instalar / Install / Instalar

Descargá **GroundedDLSS-11.24.1-beta.zip** de
[la nueva release](https://github.com/aquiyahora1598/Grounded-DLSS/releases/tag/v11.24.1-beta).
**Extraé TODO el ZIP a una carpeta** y ejecutá el instalador que contiene.
Dejá **GroundedDLSS-payload.zip junto al EXE**, sin extraer ese ZIP interno.
Con el juego cerrado, elegí idioma y la carpeta que contiene Grounded.exe,
revisá los términos y seleccioná **Instalar / actualizar**. Iniciá con DirectX 12.

**EN:** Download the full release ZIP, extract everything, and run the included
Setup. Keep GroundedDLSS-payload.zip beside the EXE; do not extract the inner ZIP.
**PT-BR:** Baixe o ZIP completo, extraia tudo e execute o instalador incluído.
Mantenha GroundedDLSS-payload.zip ao lado do EXE, sem extrair o ZIP interno.

**11.24.1 cambia la distribución y el instalador. El runtime sigue en 11.24.0.**
Los archivos publicados conservan los bytes exactos de la variante B probada:
por eso el EXE interior aún se llama
`GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`.
[Guía completa / Full guide / Guia](INSTALLER_B.md).

Para desinstalar, cerrá Grounded y elegí **Desinstalar / Uninstall** en ese mismo
EXE o en `Maine/Binaries/Win64/GroundedDLSS-install/Desinstalar Grounded DLSS.exe`.
Retira los archivos AMD que instaló y conserva archivos modificados por el usuario.
Restaura los ajustes propios sin perder otros cambios del juego.

Incluye dos DLL oficiales AMD firmados y su licencia original. **No incluye ni
descarga DLL NVIDIA.** En cada arranque busca el DLSS SR de producción compatible
más nuevo que ya ofrezcan los componentes locales de NVIDIA, verifica su firma y
controlador mínimo y prepara una copia de trabajo. Necesita esa fuente local;
no usa una biblioteca privada de respaldo. No cambia la caché ni los perfiles NVIDIA.

| Escala del juego | DLSS |
|---|---|
| 100 | DLAA |
| 67 | Calidad / Quality / Qualidade |
| 58 | Equilibrado / Balanced |
| 50 | Rendimiento / Performance / Desempenho |
| 33 | Ultra Rendimiento / Ultra Performance |

Elige el nivel más cercano. Nitidez desactivada. F9 funciona con esos modos.
Esta beta prepara FG en el pase DLSS: **F8 también suspende FG al apagar DLSS**;
retoma al reactivar DLSS si F9 seguía encendido. En pausa, cargas y menús se suspende FG.
Para iniciar sin FG, agregá `[FrameGeneration]` y `Enabled=0` al INI del mod.

## Pruebas y límites

Probado dentro de Grounded en RTX 4070 Laptop a 1080p y 1440p; pausa/reanudación,
F8/F9 y cambios de resolución. La presentación aislada pasó también a 4K.
A 1440p con DLAA, una muestra corta pasó de unos 67 cuadros/s sin FG a 101
presentaciones/s con FG (50 renderizados más los generados). Son contadores DXGI,
no scanout o latencia ni una garantía para otras escenas. El usuario confirmó
una prueba satisfactoria. Puede haber artefactos breves al entrar en pausa.
Ultra puede perder detalle; HDR y compatibilidad universal no están validados.
FG no duplica la respuesta de los controles. Esta versión es beta. La variante B pasó además instalación/desinstalación y las cinco comprobaciones F8/F9 dentro del juego; [alcance de la validación](docs/INSTALLER_B_VALIDATION.md).

[Español](installer/LEEME.txt) · [English](installer/LEEME.en.txt) ·
[Português (Brasil)](installer/LEEME.pt-BR.txt).

## Compilar

Python 3.10+, MSVC C++ x64, Windows SDK y .NET Framework 4.x:

1. Leé la licencia NVIDIA incluida. Si aceptás sus términos:
   `python tools/fetch_sdk.py --accept-nvidia-sdk-license`.
2. `python tools/fetch_framegen_sdks.py amd` obtiene y verifica el SDK oficial AMD fijado.
3. `python tools/build.py loader` y `python tools/build.py dlss-fsr-framegen`.
4. `python tools/build_release.py` y `python tools/build_source_release.py` generan candidatos.

El control `--public` sigue exigiendo resolver con evidencia `publication/clearance.json`.
Esta beta se comparte por solicitud expresa del autor, sin declarar resueltos los
pendientes documentados en [LEGAL.md](LEGAL.md). No es aprobación de los fabricantes.
No subas la carpeta de investigación: solo la exportación de fuentes.

## Avisos

This software contains source code provided by NVIDIA Corporation.

[Licencia AMD original](licenses/AMD-FidelityFX-SDK-LICENSE.txt), incluyendo los términos
de redistribución binaria y excepciones MIT para encabezados. La MIT del proyecto
no cambia condiciones de NVIDIA, AMD ni derechos del juego. Sin afiliación con
NVIDIA, AMD, Obsidian, Xbox o Valve.

Grounded DLSS isn't endorsed by Obsidian Entertainment, Inc. and doesn't reflect
the views or opinions of Obsidian Entertainment, Inc. or anyone officially involved
with making Grounded. Grounded and Obsidian Entertainment and all related logos
are trademarks or registered trademarks of Obsidian Entertainment, Inc.

Revisá los registros antes de compartirlos: pueden contener rutas personales.
No se publican partidas, capturas del juego ni volcados de memoria.
