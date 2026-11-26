#pragma once

#include <android/native_activity.h>

#include <string>

namespace lex {

// Registro: logcat (etiqueta LeX) y el mismo lex.log que la parte Java.
void log(const char* format, ...);
void setLogFile(const std::string& path);

// GOT: sustituye en una libreria cargada la direccion de una funcion importada.
// Devuelve la original (o nullptr si la libreria no la importa).
void* hookImport(const char* library, const char* symbol, void* replacement);

// Panel, HUD y entrada (overlay.cpp).
void installOverlay(const char* gameLibrary, ANativeActivity* activity);

}  // namespace lex
