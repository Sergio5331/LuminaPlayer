# Contribuir a LuminaPlayer

Para comunicar un fallo, incluye pasos de reproducción, el comportamiento esperado y la versión de Windows. Si afecta a un archivo multimedia, indica su formato y códec; no adjuntes contenido que no tengas permiso para compartir.

Para proponer un cambio, describe el problema que resuelve y mantén el alcance concreto. Compila la aplicación y ejecuta las pruebas de integración cuando el cambio afecte a reproducción, controles o persistencia. Las instrucciones se encuentran en README.md.

El proyecto utiliza C++20, Qt 6 Widgets y libmpv. Mantén la gestión de recursos mediante RAII y evita bloquear el hilo de interfaz. Los cambios en capturas o miniaturas deben conservar la reproducción principal y respetar la selección de subtítulos.

No incluyas dependencias descargadas, ejecutables, certificados, claves, capturas personales ni diagnósticos con información privada.
