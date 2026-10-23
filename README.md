# Grounded DLSS + FSR Frame Generation

<!-- antivirus-notice-11.24.0 -->
> [!WARNING]
> **Antivirus / Security / Segurança — 11.24.0 Beta · 23/10/2026**
>
> **ES:** Se reportó un bloqueo de Defender como `Trojan:Win32/Sabsik.FL.A!ml`.
> **VirusTotal: 9/70 motores lo marcaron.** Puede haber falsos positivos, pero
> todavía no están confirmados; la revisión solicitada a Microsoft sigue sin dictamen.
> Si el antivirus lo bloquea, mantenelo bloqueado: no desactives la protección ni añadas exclusiones.
>
> **EN:** Defender blocks have been reported. **VirusTotal: 9/70 detections.**
> False positives are possible but unconfirmed; a Microsoft review has been requested.
> Keep blocked files blocked. Do not disable protection or add exclusions.
>
> **PT-BR:** Foram relatados bloqueios do Defender. **VirusTotal: 9/70 detecções.**
> Possíveis falsos positivos ainda não foram confirmados; foi solicitada revisão à Microsoft.
> Mantenha o arquivo bloqueado. Não desative a proteção nem adicione exclusões.
>
> [**Ver informe completo de VirusTotal / Full report / Relatório completo**](https://www.virustotal.com/gui/file/88db9f4431bea059adf86b9825abb1c2fde18edf6eb5e1930bd4508198edf485/detection)
> · [**Detalles, hash y estado / Details / Detalhes**](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.md).
> El resultado corresponde al instalador exacto y no certifica su seguridad / This is not a safety certificate / Não é um certificado de segurança.
<!-- /antivirus-notice-11.24.0 -->

**11.24.0 Beta · Español / English / Português (Brasil)**

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

Descargá **GroundedDLSS-11.24.0-beta-Setup.exe** de
[Releases](https://github.com/aquiyahora1598/Grounded-DLSS/releases/tag/v11.24.0-beta).
Con el juego cerrado, elegí idioma y carpeta Grounded, revisá los términos y
seleccioná **Instalar / actualizar · Install / update · Instalar / atualizar**.
Iniciá el juego con DirectX 12.

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
FG no duplica la respuesta de los controles. Esta versión es beta.

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
