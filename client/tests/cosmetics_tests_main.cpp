// Carga el catalogo de cosmeticos con el mismo codigo que la DLL. Uso:
//   lex-cosmetics-tests [carpeta de datos]   (por defecto %APPDATA%\lex-client)
// Si no hay catalogo, no falla: solo se comprueba lo que existe.
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "platform/windows/game/Cosmetics.h"

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  fs::path dir;
  if (argc > 1) dir = argv[1];
  else if (const char* app = std::getenv("APPDATA")) dir = fs::path(app) / "lex-client";
  if (dir.empty() || !fs::exists(dir / "cosmetics" / "catalog.json")) {
    std::cout << "sin catalogo: nada que comprobar\n";
    return 0;
  }
  auto& cosmetics = lex::platform::windows::game::Cosmetics::instance();
  cosmetics.load(dir);
  int animated = 0;
  for (const auto& item : cosmetics.items()) {
    if (item.anim.empty()) continue;
    ++animated;
    for (const auto& [bone, rows] : item.anim) {
      if (static_cast<int>(rows.size()) != item.animFrames) {
        std::cout << "FALLO " << item.id << ": " << bone << " tiene " << rows.size() << " fotogramas\n";
        return 1;
      }
    }
  }
  std::cout << "cosmeticos: " << cosmetics.items().size() << ", con vista previa animada: " << animated << "\n";
  return cosmetics.items().empty() ? 1 : 0;
}
