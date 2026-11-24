add_library(lex-core STATIC
  ClientContext.h
  ClientContext.cpp
  module/Category.h
  module/Module.h
  module/Module.cpp
  module/ModuleManager.h
  module/ModuleManager.cpp
  settings/Types.h
  settings/Types.cpp
  settings/Setting.h
  settings/SettingGroup.h
  settings/SettingVisitor.h
  settings/SettingVisitor.cpp
  events/Events.h
  events/EventBus.h
  events/EventBus.cpp
  config/ConfigManager.h
  config/ConfigManager.cpp
  platform/IPlatform.h
  gui/GuiTheme.h
  gui/GuiTheme.cpp
  gui/ClickGuiModel.h
  gui/ClickGuiModel.cpp
  util/Logger.h
  util/Logger.cpp
  util/Math.h
  util/Strings.h
)

target_include_directories(lex-core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-core PUBLIC nlohmann_json::nlohmann_json)
