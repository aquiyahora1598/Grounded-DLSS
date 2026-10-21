## Revision actual 11.23.4: idiomas del instalador

Se traduce la interfaz, los mensajes y los documentos propios al ingles y
portugues de Brasil. La licencia NVIDIA, MIT y avisos originales se conservan
sin cambios. Las traducciones propias identifican el original espanol y no
reemplazan los terminos NVIDIA. El runtime sigue en 11.23.2. La solicitud
del autor de publicar no resuelve por si sola los pendientes documentados.

## Revision 11.23.3: FSR desactivado por el instalador

El instalador apaga FSR automaticamente, conserva la seleccion anterior y la
restaura al desinstalar respetando cambios posteriores del usuario. El runtime
permanece en 11.23.2, con el mismo binario. No se modifican la procedencia de
DLSS, las licencias ni los pendientes de publicacion. Ver installer/VALIDACION.txt.

## Revision 11.23.2: correccion de calidad de imagen

Retira el reinicio de historial DLSS que 11.23.1 aplicaba ante pases temporales
no clasificados y devuelve el tiempo entre fotogramas al de 11.23.0. No cambia
la procedencia de la biblioteca NVIDIA ni los pendientes de publicacion.
Ver installer/VALIDACION.txt.

## Historico de revisiones legales y tecnicas

## Revision 11.23.1: correcciones tecnicas de la auditoria

Corrige estabilidad del runtime, el redondeo de Ultra a 1440p, la recuperacion
del instalador y la conservacion de Engine.ini. No cambia la procedencia de la
biblioteca NVIDIA: sigue siendo solo la fuente local firmada, sin descarga ni
respaldo, como en 11.23.0. La revision de publicacion ahora exige la evidencia
GPU del ASI exacto. Las condiciones NVIDIA y los pendientes de publicacion
permanecen. Ver installer/VALIDACION.txt.

## Revision 11.23.0: fuente NVIDIA local, sin descarga ni respaldo DLSS

Se retiraron el descargador, el enlace de descarga, el selector manual y la
instalacion de la DLL privada de respaldo. El mod exige una biblioteca de
produccion firmada por NVIDIA en la cache NGX o en el paquete activo del driver.
La unica copia generada es la de trabajo necesaria para la carga de NGX; no se
usa como fuente cuando falta el componente NVIDIA. La actualizacion retira una
DLL de respaldo anterior solo si coincide con el registro de instalacion.
Los respaldos de configuracion y la recuperacion de transacciones se conservan
para restaurar los ajustes del usuario. Las condiciones NVIDIA y los pendientes
de publicacion permanecen. Las menciones de descargas o DLL de respaldo en las
revisiones historicas siguientes describen versiones anteriores, no esta.

## Revision 11.22.0: seleccion local por firma, sin lista de versiones

Esta revision sustituye la lista cerrada de hashes para archivos locales por
verificacion Authenticode de NVIDIA, identidad DLSS SR x64 de produccion y
comparacion numerica de versiones. Consulta la cache NGX y, cuando ofrece SR,
el paquete activo del controlador. Respeta el controlador minimo que declara
la biblioteca; NGX comprueba la disponibilidad efectiva en la GPU/controlador.
La consulta se repite una vez en cada arranque. Puede copiar la nueva biblioteca
a una carpeta propia sin alterar la cache NVIDIA ni la copia de respaldo.
No descarga bibliotecas durante la partida ni cambia perfiles del controlador.

Las futuras bibliotecas locales no necesitan otra version del mod solo porque
cambien su numero o hash. Una futura incompatibilidad de ABI, de firma o del
controlador podria requerir cambios; no se garantiza compatibilidad universal.
La descarga opcional de respaldo sigue fijada y verificada, con permiso previo,
para equipos sin biblioteca local utilizable. Esta seleccion local no elimina
los pendientes de licencias, marcas y terminos del juego enumerados debajo.

Windows verifica la firma segun [WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust);
la consulta usa el almacen local y no permite recuperacion de certificados por
red. No se modifica el sistema de confianza ni se aceptan certificados propios.
Las bibliotecas se inspeccionan como datos; no se ejecutan para autenticar su firma.

# Revisión de distribución: 20 de octubre de 2026

**Estado: candidato privado. No hay publicación ni aprobación de terceros.**

Decisión del autor: instalador y código fuente gratuitos, con MIT para el código
propio. Se retira la recomendación anterior de subir el ZIP 11.21.1: contenía una
DLL redistribuible, pero no había quedado resuelto el cumplimiento completo.
Esta revisión identifica condiciones documentadas y dudas concretas; no garantiza
la legalidad en todas las jurisdicciones.

## NVIDIA: permiso condicionado, no prohibición general de GitHub

La [licencia exacta del SDK utilizado](https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/LICENSE.txt)
(14/03/2024) permite determinados componentes integrados en aplicaciones,
sujetos a sus condiciones (§§1–2). El
[manual oficial fijado](https://github.com/NVIDIA/DLSS/blob/374959484e79a640feaba44c93ac8cfb0a03f5b5/doc/DLSS_Programming_Guide_Release.pdf)
§4.2 distingue la DLL de producción redistribuible de la DLL de desarrollo,
prohibida en lanzamientos públicos. La nuestra era de producción, 310.9.1.0,
con firma NVIDIA válida; incluir un archivo de NVIDIA no es por sí solo una
infracción. Tampoco su presencia en un repositorio oficial concede derechos ilimitados.

Condiciones relevantes: atribuciones, términos de distribución protectores,
conservación de avisos, restricciones de ingeniería inversa del SDK y controles
de exportación (§§2,4,7,15). Suplemento: GPU NVIDIA (§1), notificación previa
de lanzamiento comercial —incluidos plugins de aplicaciones comerciales— (§4),
corrección de problemas materiales (§6.2), atribuciones/marcas y revisión escrita
de determinados usos (§7). No asumimos que un mod gratuito quede exento por ser gratis.

**Pendiente con NVIDIA:** aclarar la aplicación de §4 al mod gratuito y cómo
cumplir §7 en una integración no oficial sin controlar los créditos del juego.
No se presume aprobación por silencio. Quitar la DLL o descargarla al instalar
no elimina estas cuestiones. Contacto oficial de licencias: §19,
`nvidia-rtx-license-questions@nvidia.com`. No se envió ninguna consulta/notificación.

## Correcciones realizadas

- Instalador 11.21.5 sin `nvngx_dlss.dll`. Se reutiliza una copia local compatible
  del mod o de la caché NVIDIA NGX, sin modificar esa caché. Si no existe, se
  ofrece descargar desde NVIDIA/DLSS, con aceptación expresa y términos accesibles,
  o aportar manualmente la DLL de producción 310.9.1.0,
  58.956.912 bytes, SHA-256
  `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`.
  Se admiten también las copias de caché 310.9.16.0, 310.9.0.0 y 310.6.0.0
  con hashes fijados, después de verificar sus firmas NVIDIA y ejecutar pruebas
  GPU aisladas. No se aceptan bibliotecas arbitrarias o versiones no verificadas.
  No carga una DLL para comprobarla ni modifica archivos antes de esa validación.
- Términos y licencias accesibles desde el instalador, aceptación explícita para
  instalar; la desinstalación y recuperación no requieren aceptar nada nuevo.
- MIT para el código original, con alcance separado de terceros. Aviso NVAPI MIT
  agregado, además de los avisos DLSS y el texto completo de la licencia RTX.
- Se sustituyó el ID numérico NGX `608699473`, sin constancia de asignación al
  proyecto, por el GUID propio `d91874f0-6893-4ba7-ae47-a9b3384f7530` y tipo CUSTOM.
  El manual NVIDIA §5.2.1 y sus encabezados indican usar ProjectID cuando no hay
  un ApplicationID otorgado por NVIDIA. No se usa un ID de otro título como respaldo.
- Fuentes exportadas mediante lista explícita: sin encabezados NVIDIA, DLL,
  librerías, volcados, capturas, partidas, configuración personal ni SDK/herramientas
  de investigación. Los encabezados se obtienen aparte desde NVIDIA con hashes.
- Aviso exigido por la política de fans de Obsidian en README, LEEME y términos.
  Sin logos ni imágenes oficiales. No se interpreta como autorización del mod.
- Los generadores separan candidatos privados de paquetes públicos. `--public`
  exige resolución documentada de los pendientes en `publication/clearance.json`.

## NVAPI, Microsoft y otros componentes

Los [encabezados NVAPI oficiales](https://github.com/NVIDIA/nvapi/blob/70d337db9186e968eab622f7e786de7e437faf3d/nvapi.h)
son MIT; se conserva su aviso. No se distribuye `nvapi64.dll`: se usa la del
controlador instalado. Esto es distinto de DLSS/NGX, que tiene licencia propietaria.
Los 13 encabezados necesarios se verificaron byte por byte contra commits NVIDIA
fijados. El script de obtención no descarga ejecutables ni la DLL de producción.

No se incluyen OptiScaler, Ultimate ASI Loader, PresentMon, RenderDoc, REA,
FSR, XeSS ni generación de fotogramas. El cargador es el código propio del proyecto.
El shader de preparación es texto HLSL del proyecto, no un shader extraído del juego.
Los avisos de terceros del runtime NVIDIA se conservan para consulta sin afirmar
que todos esos componentes estén enlazados directamente al mod.

MSVC, Windows SDK y .NET se usan para compilar; no se empaquetan sus herramientas
ni DLL de Windows. El código objeto de CRT incorporado al compilar conserva sus
condiciones. La
[documentación de redistribución de Microsoft](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files)
remite a la licencia de las herramientas usadas; el autor debe tener los derechos
de uso/distribución correspondientes. No se etiqueta código Microsoft como MIT.

## Grounded, Steam y derechos del juego

La [política oficial de Obsidian](https://www.obsidian.net/legal/ip-use) incluye
Grounded, permite proyectos gratuitos dentro de sus reglas, exige aviso de falta
de respaldo, prohíbe usar logos sin permiso y mantiene aplicables las EULA.
También contiene derechos a favor de Obsidian e indemnidad; debe leerse completa
antes de decidir acogerse a ella. No es una autorización inequívoca de una
modificación del renderizado obtenida mediante análisis del ejecutable.

El [acuerdo de Steam](https://store.steampowered.com/subscriber_agreement/), §2.G,
restringe modificaciones e ingeniería inversa salvo permiso o excepción legal.
La EULA específica intentada desde Steam no pudo obtenerse; la ficha pública
consultada no la enlazaba. No se sustituyó por la EULA de otro juego ni se afirmó
que la ausencia del enlace significara permiso. **Pendiente:** revisar los términos
efectivamente aplicables y confirmar el alcance para esta integración, mediante
Obsidian/Xbox o revisión jurídica aplicable al lugar de distribución.

El paquete no contiene el ejecutable ni recursos completos de Grounded. Sí hay
offsets, hashes y pequeñas secuencias de bytes para verificar compatibilidad.
No se pretende relicenciar esas secuencias como código propio: su publicación y
la ingeniería inversa requieren la revisión anterior. No se añadieron mecanismos
para saltar DRM, autenticación o controles de acceso. No se promete compatibilidad
con servicios online o sistemas de protección no probados.

## GitHub, privacidad y publicación

Los [términos de GitHub](https://docs.github.com/en/site-policy/github-terms/github-terms-of-service),
§§C–D, exigen derechos suficientes sobre lo subido. No reemplazan las licencias
de terceros. El repositorio público permite vistas y forks; por eso se excluyen
SDK propietarios y datos privados. MIT permite reutilizar nuestro código, pero no
autoriza por sí sola redistribuir DLSS, el juego o sus marcas.

Los binarios propios no están firmados: se informa sin pedir desactivar seguridad.
No se añaden telemetría, cuentas, perfiles de controlador o modificaciones de
partidas. La descarga opcional comunica a GitHub IP, agente y solicitud del archivo,
según se informa en los términos; no envía partidas ni registros. Los registros de diagnóstico siguen siendo locales; deben revisarse
antes de enviarlos. La DLL instalada que funciona no fue retirada ni reemplazada
por esta revisión. No se contactó a empresas ni se publicó contenido.

Para pasar a publicación quedan las aclaraciones NVIDIA y juego indicadas, la
identidad que el autor usará si necesita enviar la notificación, y documentar el
alcance de las licencias de compilación. El registro de pendientes conserva esos
puntos abiertos; no inventa aprobaciones ni confunde una prueba técnica con permiso.

## Verificación técnica del empaquetado

Ejecutar `python tools/review_legal_release.py` registra los resultados en
`evidence/2026-10-20/legal-review/verification.json`: integridad de ZIP/recursos,
ausencia de runtime en el instalador, importación válida/rechazo de DLL dañada,
instalación y desinstalación aisladas, regresiones del instalador, manifiesto de
fuentes y bloqueo de publicación. No sustituye pruebas visuales en el juego.

La revision de ProjectID se probo en el ASI candidato exacto: cinco modos por
salida, 20 fotogramas en 1080p, 20 en 1440p y 20 en 4K. El backend tambien
confirmo la evaluacion NVIDIA con el GUID propio. El cargador conserva su binario;
la integracion cambia las llamadas de identificacion y el texto de version.
No se desplego ese ASI en el juego ni se verifico una partida con esta revision.
El primer arnes publico carecia de la DLL en la ruta privada del ASI; tras corregir
la carpeta de prueba pasaron las tres salidas. La evidencia conserva ambos casos.

La descarga del texto completo de la EULA Microsoft Build Tools 2026 desde el
directorio oficial devolvio HTTP 403 en esta revision. Se verificaron el
[directorio de licencias](https://visualstudio.microsoft.com/license-terms/) y la
documentacion de redistribucion, pero no se da por confirmada la habilitacion
individual del autor solo por tener las herramientas instaladas. Este punto
queda documentado como pendiente, sin instalar otras herramientas ni contratar licencias.

## Descarga opcional y provisión por el controlador (11.21.5)

La ruta automática enlaza al archivo de producción del repositorio oficial de
NVIDIA y exige aceptación de los términos y del aviso de descarga. La licencia
RTX §1(a) contempla instalación y uso del SDK. No se deduce una autorización
general para publicar el mod ni se cambia el estado privado por agregar esta
ruta. Tampoco se afirma que descargar por separado elimine obligaciones del SDK.
No se aloja una copia NVIDIA en el ZIP ni en el repositorio propio. No se eluden
autenticación, términos de un portal, protecciones del controlador ni restricciones
de acceso: se usa el archivo público de NVIDIA y se conserva íntegro.

El instalador fija origen HTTPS y commit, rechaza redirecciones y respuestas de
tamaño o SHA-256 distintos, permite cancelar y no escribe la biblioteca hasta
validarla. Ante error o rechazo no se modifica la instalación. La sesión de juego
no solicita descargas. La publicación mantiene los pendientes externos anteriores.

Se probó además la API NVIDIA NVSDK_NGX_UpdateFeature con nuestro ProjectID y
sin ninguna ruta adicional de DLL. La solicitud devolvió éxito, pero NGX siguió
reportando SuperSampling_Available=0 en un proceso nuevo y en una comprobación
posterior. El resultado no demuestra que la API nunca pueda entregar DLSS;
demuestra que no resuelve el primer arranque de esta integración en la PC probada.
No se usaron IDs de otros títulos ni se modificaron perfiles o restricciones.

DESCARGA OFICIAL VERIFICADA AL 20/10/2026
La descarga opcional usa DLSS 310.9.16, la última publicación oficial comprobada
al preparar este instalador. La URL de un commit y SHA-256 quedan fijados para
evitar instalar futuras versiones sin verificar. Rechazar la descarga conserva
el equipo sin cambios. Una copia local compatible evita por completo la descarga.

## Verificacion de la instalacion local, 20/10/2026 (11.21.5)
La integracion con GUID propio se instalo y probo dentro de Grounded 1.4.7.4815.
Se uso una copia de la cache NVIDIA 310.9.16, sin descargar una biblioteca para
esa instalacion. Se verificaron 8226 evaluaciones a 1080p en Rendimiento y F8
hasta Unreal y de vuelta a DLSS. El indicador NVIDIA reporto 310.9.16 y
`nvapp_override`. No se deduce de ese indicador cual modelo K/M/L fue efectivo.
La fuente de cache no se modifico. No es una nueva prueba de todas las resoluciones.

La descarga opcional ahora apunta a [DLSS 310.9.16 oficial](https://github.com/NVIDIA/DLSS/releases/tag/v310.9.16),
publicado el 16/10/2026, commit `9981c6350dc58c0ce21f024fd65e31e851c31689`.
SHA-256: `d00b5b0ab29c3ca101b70c3a5ecb3a01b7ef33766a3383a28a18623083e6123e`.
Se verifico su firma NVIDIA y que LICENSE.txt coincide con la licencia incluida.
La descarga real mediante el instalador, seguida de instalacion/desinstalacion
sobre archivos aislados, paso. La version y huella se fijan al construir el
instalador; no se promete ejecutar automaticamente futuras versiones sin verificar.
Estas pruebas tecnicas no resuelven los pendientes de publicacion anteriores.
