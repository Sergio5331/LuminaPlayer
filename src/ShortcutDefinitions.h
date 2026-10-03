#pragma once
#include <QString>
#include <QVector>
struct ShortcutDefinition { QString id, label, key; bool repeat = false; };
inline QVector<ShortcutDefinition> shortcutDefinitions() {
    return {{"open", "Abrir archivos", "Ctrl+O"}, {"pause", "Reproducir / pausa", "Space"},
        {"left", "Retroceder 5 segundos", "Left", true}, {"right", "Avanzar 5 segundos", "Right", true},
        {"volumeUp", "Subir volumen", "Up", true}, {"volumeDown", "Bajar volumen", "Down", true},
        {"capture", "Captura limpia", "S"}, {"captureSub", "Captura con subtítulos", "Shift+S"},
        {"frameNext", "Fotograma siguiente", ".", true}, {"frameBack", "Fotograma anterior", ",", true},
        {"next", "Archivo siguiente", "PgDown"}, {"previous", "Archivo anterior", "PgUp"},
        {"subEarlier", "Adelantar subtítulos", "Z", true}, {"subLater", "Retrasar subtítulos", "X", true},
        {"fullscreen", "Pantalla completa", "F11"}, {"escape", "Salir de pantalla completa", "Esc"},
        {"pip", "Ventana flotante", "Ctrl+P"}, {"library", "Biblioteca", "Ctrl+L"},
        {"captureSettings", "Ajustes de captura", "Ctrl+,"}, {"advanced", "Herramientas de reproducción", "Ctrl+T"},
        {"loop", "Marcar A / B / quitar bucle", "L"}, {"bookmark", "Guardar marcador", "Ctrl+B"}};
}
