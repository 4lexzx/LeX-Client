// Vuelca los modulos registrados como JSON: unica fuente de verdad del launcher.
#include <fstream>

#include "core/module/ModuleManager.h"
#include "platform/windows/hud/HudModule.h"

int main(int argc, char** argv) {
  if (argc < 2) return 1;
  lex::platform::windows::hud::registerAllModules();
  nlohmann::json out = nlohmann::json::array();
  for (const auto& m : lex::core::module::ModuleManager::instance().modules()) {
    out.push_back({{"name", m->name()},
                   {"description", m->description()},
                   {"category", std::string(lex::core::module::categoryName(m->category()))},
                   {"enabled", m->isEnabled()}});
  }
  std::ofstream file(argv[1], std::ios::trunc);
  file << out.dump(2) << std::endl;
  return file.good() ? 0 : 1;
}
