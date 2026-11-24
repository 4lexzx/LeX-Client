#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/ClientContext.h"
#include "core/config/ConfigManager.h"
#include "core/gui/ClickGuiModel.h"
#include "core/events/EventBus.h"
#include "core/module/ModuleManager.h"
#include "core/settings/SettingVisitor.h"
#include "core/util/Logger.h"
#include "core/util/Math.h"
#include "core/util/Strings.h"
#include "tests/NullPlatform.h"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    ++g_checks;                                                                 \
    if (!(condition)) {                                                         \
      std::cerr << "FAIL linea " << __LINE__ << ": " #condition << '\n';       \
      ++g_failures;                                                             \
    }                                                                           \
  } while (false)

using namespace lex::core;

class ProbeModule final : public module::Module {
 public:
  ProbeModule()
      : Module("ProbeModule", "Modulo de prueba del bus",
               module::Category::Hud, "icon.probe") {
    settings().add<settings::IntSetting>("Number", "Numero", 5)
        .withRange(0, 10);
  }

  int enables = 0;
  int disables = 0;
  int ticks = 0;
  int renders = 0;
  int keys = 0;

  void onEnable() override { ++enables; }
  void onDisable() override { ++disables; }
  void onTick(const events::TickEvent&) override { ++ticks; }
  void onRender(const events::RenderEvent&) override { ++renders; }
  void onKey(const events::KeyEvent&) override { ++keys; }
};

class CountingVisitor final : public settings::SettingVisitor {
 public:
  int booleans = 0;
  int integers = 0;
  int floats = 0;
  int colors = 0;
  int enums = 0;
  int keybinds = 0;

  void visit(settings::BoolSetting&) override { ++booleans; }
  void visit(settings::IntSetting&) override { ++integers; }
  void visit(settings::FloatSetting&) override { ++floats; }
  void visit(settings::ColorSetting&) override { ++colors; }
  void visit(settings::EnumSetting&) override { ++enums; }
  void visit(settings::KeybindSetting&) override { ++keybinds; }
};

void testSettings() {
  settings::SettingGroup group("Pruebas");

  auto& boolean = group.add<settings::BoolSetting>("B", "bool", false);
  auto& integer = group.add<settings::IntSetting>("I", "int", 5).withRange(0, 10);
  auto& floating = group.add<settings::FloatSetting>("F", "float", 1.0f)
                       .withRange(0.0f, 2.0f);
  auto& color = group.add<settings::ColorSetting>("C", "color", settings::Color{});
  auto& enumeration = group.add<settings::EnumSetting>("E", "enum", 0)
                          .withOptions({"Compacto", "Detallado"});
  auto& keybind = group.add<settings::KeybindSetting>("K", "key", settings::Keybind{});

  CHECK(group.settings().size() == 6);
  CHECK(group.find("I") != nullptr);
  CHECK(group.find("NoExiste") == nullptr);

  integer.setValue(999);
  CHECK(integer.value() == 10);
  integer.reset();
  CHECK(integer.value() == 5);

  floating.setValue(-1.0f);
  CHECK(floating.value() == 0.0f);

  enumeration.setValue(99);
  CHECK(enumeration.value() == 1);
  CHECK(enumeration.options().size() == 2);

  color.setValue(settings::Color::fromArgb(0xFF336699u));
  CHECK(color.value().toArgb() == 0xFF336699u);

  keybind.setValue(settings::Keybind{16, false, true, false});
  CHECK(keybind.value().shift);
  CHECK(keybind.value().toJson().contains("key"));
  CHECK(boolean.value() == false);

  CountingVisitor visitor;
  for (const std::unique_ptr<settings::SettingBase>& setting : group.settings()) {
    setting->accept(visitor);
  }
  CHECK(visitor.booleans == 1);
  CHECK(visitor.integers == 1);
  CHECK(visitor.floats == 1);
  CHECK(visitor.colors == 1);
  CHECK(visitor.enums == 1);
  CHECK(visitor.keybinds == 1);
}

void testClickGuiModel(module::ModuleManager& manager) {
  gui::ClickGuiModel model(manager);
  CHECK(!model.isOpen());
  CHECK(model.theme().menuKey.key == 0xA1);
  CHECK(model.theme().menuKey.shift);

  model.toggle();
  CHECK(model.isOpen());
  model.update(1.0f);
  CHECK(model.openAmount() > 0.9f);

  model.setTab(gui::ClickGuiModel::Tab::Hud);
  CHECK(!model.visibleModules().empty());

  model.setSearch("  PROBE  ");
  CHECK(model.visibleModules().size() == 1);
  model.setSearch("no-existe");
  CHECK(model.visibleModules().empty());
  model.setSearch("");

  model.toggleFavorite("ProbeModule");
  CHECK(model.isFavorite("ProbeModule"));
  model.setTab(gui::ClickGuiModel::Tab::Favorites);
  CHECK(model.visibleModules().size() == 1);
  model.toggleFavorite("ProbeModule");
  CHECK(!model.isFavorite("ProbeModule"));

  model.selectModule("ProbeModule");
  CHECK(model.settingsOpen());
  CHECK(model.selectedModule() != nullptr);
  model.closeSettings();
  CHECK(!model.settingsOpen());

  const nlohmann::json saved = model.toJson();
  CHECK(saved.contains("theme"));
  CHECK(saved.contains("favorites"));
  gui::ClickGuiModel restored(manager);
  restored.fromJson(saved);
  CHECK(restored.isOpen());
  CHECK(restored.tab() == model.tab());
  CHECK(restored.theme().menuKey.key == model.theme().menuKey.key);

  model.setOpen(false);
  model.update(5.0f);
  CHECK(!model.isVisible());
}

void testEventBus() {
  events::EventBus bus;
  int calls = 0;

  auto subscription = bus.subscribe<events::TickEvent>(
      [&calls](events::TickEvent&) { ++calls; });
  events::TickEvent tick;
  bus.publish(tick);
  CHECK(calls == 1);

  subscription.reset();
  bus.publish(tick);
  CHECK(calls == 1);
  CHECK(bus.listenerCount() == 0);

  int blockedCalls = 0;
  auto canceller = bus.subscribe<events::TickEvent>([](events::TickEvent& event) {
    event.cancelled = true;
  });
  auto blocked = bus.subscribe<events::TickEvent>(
      [&blockedCalls](events::TickEvent&) { ++blockedCalls; });
  bus.publish(tick);
  CHECK(blockedCalls == 0);
  canceller.reset();
  blocked.reset();
}

void testUtil() {
  CHECK(util::clamp(5, 0, 3) == 3);
  CHECK(util::lerp(0.0f, 10.0f, 0.25f) == 2.5f);
  CHECK(util::toLower("LeX-ClIeNt") == "lex-client");
  CHECK(util::trim("  hola  ") == "hola");
  CHECK(util::split("a,b,c", ',').size() == 3);
  util::Logger::instance().info("Prueba de logger (Fase 1)");
}

}  // namespace

int main() {
  using namespace lex::core;

  const std::filesystem::path configDirectory =
      std::filesystem::temp_directory_path() / "lex-client-fase1-tests";
  std::error_code error;
  std::filesystem::remove_all(configDirectory, error);

  CHECK(module::ModuleManager::instance().count() >= 1);
  CHECK(module::ModuleManager::instance().find("TestModule") != nullptr);

  testSettings();
  testEventBus();
  testUtil();

  auto& manager = module::ModuleManager::instance();
  auto probe = std::make_unique<ProbeModule>();
  ProbeModule* probePointer = probe.get();
  manager.registerModule(std::move(probe));

  testClickGuiModel(manager);

  lex::tests::NullPlatform platform;
  events::EventBus bus;
  config::ConfigManager config(configDirectory);
  config.load();
  manager.attachAll(platform, bus, config);

  probePointer->setEnabled(true);
  CHECK(probePointer->enables == 1);
  CHECK(bus.listenerCount() == 5);

  events::TickEvent tick;
  events::RenderEvent render;
  events::KeyEvent key;
  bus.publish(tick);
  bus.publish(render);
  bus.publish(key);
  CHECK(probePointer->ticks == 1);
  CHECK(probePointer->renders == 1);
  CHECK(probePointer->keys == 1);

  probePointer->setEnabled(false);
  CHECK(probePointer->disables == 1);
  bus.publish(tick);
  bus.publish(render);
  bus.publish(key);
  CHECK(probePointer->ticks == 1);
  CHECK(probePointer->renders == 1);
  CHECK(probePointer->keys == 1);
  CHECK(bus.listenerCount() == 0);

  // Persistencia: enabled + settings + keybind, perfiles default/stream.
  probePointer->setEnabled(true);
  auto* number = static_cast<settings::IntSetting*>(
      probePointer->settings().find("Number"));
  number->setValue(7);
  manager.saveToConfig(config);
  CHECK(config.save());
  CHECK(std::filesystem::exists(config.currentPath()));
  CHECK(config.findModule("ProbeModule") != nullptr);

  probePointer->setEnabled(false);
  number->setValue(2);
  CHECK(config.load());
  manager.loadFromConfig(config);
  CHECK(probePointer->isEnabled());
  CHECK(number->value() == 7);

  config.setProfile("stream");
  config.markDirty();
  CHECK(config.save());
  const std::vector<std::string> profiles = config.availableProfiles();
  CHECK(std::find(profiles.begin(), profiles.end(), "default") != profiles.end());
  CHECK(std::find(profiles.begin(), profiles.end(), "stream") != profiles.end());

  ClientContext context;
  CHECK(!context.initialized());
  context.initialize(platform, configDirectory, "default");
  CHECK(context.initialized());
  context.tick(1.0f / 60.0f);
  context.shutdown();
  CHECK(!context.initialized());

  manager.detachAll();
  std::filesystem::remove_all(configDirectory, error);

  std::cout << "\nFase 1: " << g_checks << " comprobaciones, " << g_failures
            << " fallos.\n";
  return g_failures == 0 ? 0 : 1;
}
