# LuminaPlayer

<p align="center"><img src="assets/lumina-logo.png" width="140" alt="Logo de LuminaPlayer"></p>

Reproductor multimedia para Windows, desarrollado en C++20 con Qt 6 Widgets y libmpv. Interfaz oscura, controles flotantes y una ventana compacta para seguir viendo vídeos mientras trabajas.

[**Descargar LuminaPlayer para Windows x64**](https://github.com/Sergio5331/LuminaPlayer/releases/download/v0.9.0-beta.1/LuminaPlayer-0.9.0-Beta-Setup-x64.exe) · [Reportar un problema](https://github.com/Sergio5331/LuminaPlayer/issues)

**Versión 0.9.0 Beta.** Código abierto bajo GPLv3 o posterior. Incluye pruebas locales; todavía requiere validación en otros equipos antes de considerarse una versión estable.

## Descargar e instalar

1. Descarga el instalador desde el enlace anterior o desde [Releases](https://github.com/Sergio5331/LuminaPlayer/releases/tag/v0.9.0-beta.1).
2. Ejecuta `LuminaPlayer-0.9.0-Beta-Setup-x64.exe` y sigue el asistente en español.
3. Puedes crear un acceso directo y activar la integración con **Abrir con**.
4. Abre LuminaPlayer desde el menú Inicio y arrastra un vídeo o audio a la ventana.

Compatible con Windows 10/11 de 64 bits. La instalación es por usuario y no solicita permisos de administrador. El instalador incluye Qt, libmpv y el runtime de C++; no necesitas instalarlos por separado. Se desinstala desde Configuración → Aplicaciones. Tus capturas y ajustes se conservan.

Para elegirlo como reproductor predeterminado, utiliza **Abrir con → Elegir otra aplicación → Siempre**, o Configuración → Aplicaciones → Aplicaciones predeterminadas. El instalador no cambia tus preferencias automáticamente.

**Firma de desarrollo:** el certificado es autofirmado y no es un certificado de editor reconocido. Windows puede mostrar advertencias o bloquear la ejecución. El instalador no modifica las protecciones de Windows.

Las capturas nuevas se guardan en `Imágenes/LuminaPlayer/capturas`, salvo que hayas elegido otra carpeta. Las fuentes de las bibliotecas se incluyen en la instalación para acompañar sus licencias.

## Funciones

- Reproducción de archivos de vídeo y audio mediante libmpv, con decodificación por hardware `auto-safe`, mapeo HDR a SDR y normalización de audio.
- Controles de reproducción y volumen, búsqueda temporal, pantalla completa y ocultamiento automático del cursor y de los controles.
- Ventana flotante siempre al frente, con solo el vídeo visible, movimiento mediante arrastre y cambio de tamaño conservando la proporción.
- Miniaturas al recorrer la barra de progreso, con precarga, caché y un proceso auxiliar de extracción de fotogramas.
- Capturas PNG/JPG a resolución del vídeo, con o sin subtítulos externos, nombres con tiempo de reproducción e historial de capturas.
- Subtítulos ASS/SSA, selección de pistas, ajustes de tamaño, posición y sincronización.
- Listas de reproducción, archivos recientes, carpetas, listas M3U/M3U8 y recuperación de la posición de reproducción.
- Velocidad de reproducción, modos de audio, capítulos, marcadores y repetición A–B.
- Atajos configurables, teclas multimedia, apertura por arrastrar y soltar e instancia única.
- Registro local de errores y exportación de diagnóstico.

La disponibilidad de códecs y aceleración depende de la compilación de libmpv, la GPU y sus controladores. La captura limpia excluye subtítulos renderizados por el reproductor; los subtítulos o marcas ya grabados en los píxeles del vídeo permanecen. Las miniaturas pueden mostrar inicialmente un fotograma cercano mientras se obtiene el instante solicitado.

## Requisitos de compilación

- Windows x64.
- Visual Studio 2022 / Build Tools con C++ de escritorio y Windows SDK; alternativamente clang-cl con ABI compatible con MSVC.
- CMake 3.20 o posterior.
- Qt 6.5 o posterior para MSVC x64: Core, Gui, Widgets y Network. Qt Test es necesario para las pruebas.
- SDK de libmpv x64: cabeceras `include/mpv`, biblioteca de importación MSVC en `lib` y `libmpv-2.dll` o `mpv-2.dll`.

Se ha verificado localmente con Qt 6.8.3 y MSVC 2022. Las cabeceras, la biblioteca de importación y la DLL de libmpv deben corresponder a una misma distribución y arquitectura.

## Compilar

Desde una terminal de herramientas de Visual Studio x64, configura las rutas correspondientes a tus SDK:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/SDK/Qt/6.8.3/msvc2022_64" -DMPV_ROOT="C:/SDK/mpv"
cmake --build build --config Release --parallel
```

CMake copia libmpv y ejecuta `windeployqt` de forma predeterminada. Mantén `LuminaThumbnail.exe` y las DLL desplegadas junto a `LuminaPlayer.exe`.

```powershell
.\build\Release\LuminaPlayer.exe
.\build\Release\LuminaPlayer.exe "C:\Videos\pelicula.mkv"
```

Para utilizar `build.ps1`, coloca Qt en `third_party/Qt` y el SDK de libmpv en `third_party/mpv`:

```powershell
.\build.ps1
.\build.ps1 -Tests
```

## Pruebas

También pueden ejecutarse con CMake, usando Qt Test:

```powershell
cmake -S . -B build -DLUMINA_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Las pruebas requieren un escritorio Windows interactivo. Comprueban reproducción, capturas, miniaturas, controles, persistencia de ventanas, listas e instancia única. No constituyen una medición de rendimiento 4K ni una certificación de compatibilidad con todas las GPU.

## Atajos predeterminados

| Acción | Atajo |
| --- | --- |
| Abrir archivos | Ctrl+O |
| Reproducir / pausa | Espacio |
| Retroceder / avanzar 5 segundos | Izquierda / Derecha |
| Volumen | Arriba / Abajo o rueda |
| Captura limpia / con subtítulos | S / Shift+S |
| Fotograma siguiente / anterior | . / , |
| Archivo siguiente / anterior | Page Down / Page Up |
| Sincronizar subtítulos | Z / X |
| Pantalla completa | F11 o doble clic |
| Salir de pantalla completa | Esc |
| Ventana flotante | Ctrl+P |
| Biblioteca | Ctrl+L |
| Ajustes de captura | Ctrl+, |
| Herramientas de reproducción | Ctrl+T |
| Repetición A–B | L |
| Guardar marcador | Ctrl+B |

En la ventana flotante, arrastra el centro del vídeo para moverla y los bordes o esquinas para redimensionarla. El menú contextual permite restaurar la ventana y controlar la reproducción.

## Estructura

```text
assets/                 Logo e icono de Windows
src/                    Aplicación y proceso auxiliar de miniaturas
tests/                  Pruebas de integración
CMakeLists.txt          Configuración de compilación
build.ps1               Compilación con SDK locales
```

Los directorios de compilación, dependencias, capturas y certificados locales quedan excluidos del repositorio. Los diagnósticos pueden contener rutas de archivos: revísalos antes de compartirlos.

## Distribución

El instalador se crea con Inno Setup 6.6 o posterior y PowerShell 7. Para prepararlo desde el código fuente, instala Qt y Visual Studio, además de 7-Zip:

```powershell
.\prepare-installer.ps1
.\build-installer.ps1
```

El primer comando obtiene el [paquete de dependencias y fuentes](https://github.com/Sergio5331/LuminaPlayer/releases/download/v0.9.0-beta.1/LuminaPlayer-Dependencies.zip), verifica su SHA256 y genera la biblioteca de importación MSVC. El segundo compila, ejecuta las pruebas y crea el instalador en `dist/`. Para usar un compilador de Inno Setup instalado en otra ruta, añade `-InnoCompiler "C:/Herramientas/Inno Setup 6/ISCC.exe"`.

`-Sign` utiliza un certificado local de desarrollo mediante `sign-local.ps1`. Las claves privadas y las contraseñas quedan excluidas del repositorio y del instalador. No establece confianza en Windows.

La publicación incluye `SHA256SUMS.txt` para verificar los archivos descargados.

## Licencia

El código de LuminaPlayer se distribuye bajo **GNU GPL versión 3 o posterior**; consulta [LICENSE](LICENSE). Puedes obtener el código desde **Code → Download ZIP** o clonar el repositorio.

Qt, libmpv y las demás dependencias conservan sus propias licencias. El paquete de esta beta utiliza libmpv con `gpl=false` y FFmpeg bajo LGPLv3 o posterior. Los avisos y textos de licencia se encuentran en `packaging/` y también se instalan con la aplicación. Consulta los proyectos oficiales: [Qt](https://www.qt.io/), [mpv](https://mpv.io/) y [libass](https://github.com/libass/libass).

Desarrollado por [Sergio](https://github.com/Sergio5331).
