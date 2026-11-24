#pragma once

#include "core/module/Module.h"

namespace lex::modules::test {

// Modulo de humo de la Fase 1: exercise los 6 tipos de Setting y el ciclo
// de vida. Se auto-registra con la macro al final del .cpp.
class TestModule final : public core::module::Module {
 public:
  TestModule();

  void onEnable() override;
  void onDisable() override;
  void onTick(const core::events::TickEvent& event) override;
  void onRender(const core::events::RenderEvent& event) override;

 private:
  int ticksSeen_ = 0;
};

}  // namespace lex::modules::test
