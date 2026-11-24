#include "modules/test/TestModule.h"

#include "core/module/ModuleManager.h"
#include "core/platform/IPlatform.h"
#include "core/util/Logger.h"
#include "core/util/Math.h"

namespace lex::modules::test {

using namespace lex::core::settings;

TestModule::TestModule()
    : Module("TestModule", "Modulo de humo del nucleo (Fase 1)",
             core::module::Category::Utility, "icon.module.test") {
  settings().add<BoolSetting>("Verbose", "Registrar cada intervalo", true);
  settings().add<IntSetting>("Interval", "Cada N ticks", 60)
      .withRange(1, 600)
      .withStep(1);
  settings().add<FloatSetting>("Scale", "Escala de prueba", 1.0f)
      .withRange(0.5f, 2.0f)
      .withStep(0.05f);
  settings().add<ColorSetting>("Accent", "Color de acento",
                               Color{0.36f, 0.64f, 1.0f, 1.0f});
  settings().add<EnumSetting>("Mode", "Modo de prueba", 0)
      .withOptions({"Compacto", "Detallado"});
  settings().add<KeybindSetting>("ToggleKey", "Tecla de prueba", Keybind{});
}

void TestModule::onEnable() {
  ticksSeen_ = 0;
  LEX_LOG_DEBUG("TestModule::onEnable");
}

void TestModule::onDisable() {
  LEX_LOG_DEBUG("TestModule::onDisable (ticks=" + std::to_string(ticksSeen_) + ")");
}

void TestModule::onTick(const core::events::TickEvent& event) {
  (void)event;
  ++ticksSeen_;

  const auto* verbose = static_cast<const BoolSetting*>(settings().find("Verbose"));
  const auto* interval = static_cast<const IntSetting*>(settings().find("Interval"));
  if (verbose != nullptr && verbose->value() && interval != nullptr &&
      ticksSeen_ % interval->value() == 0) {
    LEX_LOG_DEBUG("TestModule tick=" + std::to_string(ticksSeen_));
  }
}

void TestModule::onRender(const core::events::RenderEvent& event) {
  (void)event;
  // El primitive es no-op en Fase 1; sirve para verificar el contrato.
  if (platform() != nullptr) {
    platform()->drawText(core::Vec2{24.0f, 24.0f}, "LeX-Client Fase 1",
                         0xFFFFFFFFu, 16.0f);
  }
}

}  // namespace lex::modules::test

using lex::modules::test::TestModule;
LEX_REGISTER_MODULE(TestModule)
