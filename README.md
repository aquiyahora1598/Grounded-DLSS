# Grounded DLSS

**11.23.4 beta: revision de licencias y marcas pendiente.**
Actualizacion del instalador; runtime DLSS 11.23.2 sin cambios.

Instalador en **espanol, English y portugues do Brasil**, con selector de idioma.
[English help](installer/LEEME.en.txt) · [Ajuda em portugues do Brasil](installer/LEEME.pt-BR.txt).

Integracion no oficial para Grounded Steam 1.4.7.4815, Windows x64, DirectX 12 y
NVIDIA RTX. Codigo propio MIT; los componentes NVIDIA conservan sus condiciones.
No incluye DLL NVIDIA ni OptiScaler. No descarga DLSS ni conserva una biblioteca
de respaldo. Necesita una fuente DLSS SR de produccion compatible en la cache
NGX de NVIDIA o en el paquete activo de su controlador.

En cada arranque elige la version local mas reciente admitida, valida la firma
NVIDIA y comprueba el controlador minimo declarado. NGX confirma disponibilidad.
Prepara una sola copia de trabajo para cargarla; esa copia no se usa como fuente
si los componentes NVIDIA dejan de ofrecer la biblioteca. No modifica la cache
NVIDIA, los perfiles del controlador ni las partidas. Reinicia el juego para
adoptar una biblioteca que NVIDIA haya actualizado.

## Instalar y usar

Con Grounded cerrado, abri el instalador, elegi la carpeta Grounded, lee y acepta
los terminos y licencias, y elegi Instalar / actualizar. Si falta DLSS local,
avisa y no modifica la instalacion. El instalador deja FSR desactivado
automaticamente y conserva su seleccion anterior. Inicia con DirectX 12.
Si reactivas FSR en el menu, desactivalo para usar DLSS.

| Escala del juego | Modo |
|---|---|
| 100 | DLAA |
| 67 | Calidad |
| 58 | Equilibrado |
| 50 | Rendimiento |
| 33 | Ultra Rendimiento |

La escala se ajusta al valor mas cercano. F8 alterna con el pase de Unreal.
Nitidez desactivada. Ultra puede verse muy blando; la apariencia espacial de la
profundidad de campo sigue en evaluacion. No se garantiza compatibilidad universal.

Para desinstalar, cerra el juego y elegi Desinstalar en el mismo instalador o en
`Maine/Binaries/Win64/GroundedDLSS-install/Desinstalar Grounded DLSS.exe`.
Se conservan cambios del usuario y respaldos de configuracion para restaurar sus
ajustes, incluida la seleccion anterior de FSR. Si cambiaste FSR despues de
instalar, se respeta esa nueva eleccion. Resolucion y otros ajustes del menu
se conservan. La copia de trabajo propia y valida se elimina; la cache NVIDIA se conserva.
Al actualizar se retira la biblioteca privada de versiones anteriores que siga
coincidiendo con su registro. Los archivos modificados o ajenos se preservan.

Lee `installer/LEEME.txt`, `installer/TERMINOS.txt` y `LEGAL.md`.

## Compilar desde las fuentes

Necesitás Python 3.10 o posterior, Visual Studio Build Tools con C++ x64,
Windows SDK y .NET Framework 4.x para el instalador. Usá herramientas con sus
licencias correspondientes. No se redistribuyen compiladores ni SDK de Microsoft.

1. Leé `licenses/NVIDIA-RTX-SDK-LICENSE.txt`. Si aceptás sus términos, ejecutá
   `python tools/fetch_sdk.py --accept-nvidia-sdk-license`. Descarga solo los
   encabezados desde commits oficiales fijados y verifica cada SHA-256.
2. Ejecutá `python tools/build.py loader` y `python tools/build.py dlss`.
3. Ejecutá `python tools/build_release.py` para el instalador privado y
   `python tools/build_source_release.py` para exportar las fuentes.
4. `python tools/review_legal_release.py --runtime RUTA_DLSS_LOCAL` comprueba
   ambos paquetes en archivos aislados. Usa una biblioteca ya presente en los
   componentes NVIDIA; no abre el juego, no descarga DLL ni modifica una instalacion real.

La opción `--public` queda bloqueada mientras los puntos de
`publication/clearance.json` no estén resueltos con evidencia. Este control evita
confundir un candidato técnico con una autorización legal; no sustituye esa revisión.

No subas la carpeta de investigación original: solo la exportación de fuentes.
No incluyas `external`, capturas, shaders extraídos, volcados de memoria, partidas,
rutas de usuarios, SDK privados, instaladores anteriores ni herramientas auxiliares.

## Derechos y avisos

This software contains source code provided by NVIDIA Corporation.

Los términos NVIDIA siguen aplicándose aunque su DLL se obtenga por separado.
No se conceden derechos sobre Grounded ni sus firmas de compatibilidad bajo MIT.
No se incluyen logos, archivos del juego ni funciones para eludir DRM o autenticación.
El mod no está afiliado a NVIDIA, Obsidian, Xbox o Valve.

Grounded DLSS isn't endorsed by Obsidian Entertainment, Inc. and doesn't reflect
the views or opinions of Obsidian Entertainment, Inc. or anyone officially involved
with making Grounded. Grounded and Obsidian Entertainment and all related logos
are trademarks or registered trademarks of Obsidian Entertainment, Inc.

Antes de compartir registros revisá que no contengan rutas o datos personales.

