add_library(lex-platform-windows STATIC
  Exports.h
  WindowsPlatform.h
  WindowsPlatform.cpp
  hooks/HookSink.h
  hooks/VtableHook.h
  hooks/VtableHook.cpp
  hooks/DxgiHook.h
  hooks/DxgiHook.cpp
  gui/IconAtlas.h
  gui/IconAtlas.cpp
  resources/IconResource.h
  hooks/QueueHook.cpp
  input/Input.cpp
  input/GameInputGuard.cpp
  game/Telemetry.cpp
  game/Cosmetics.cpp
  game/Memory.cpp
  game/ItemPhysics.cpp
  game/PlayerList.cpp
  game/LiveTextures.cpp
  gui/Preview.cpp
  hud/Crosshair.cpp
  gui/TextureStore.cpp
  hud/HudModule.cpp
  hud/Modules.cpp
  gui/ClickGuiRenderer.h
  gui/ClickGuiRenderer.cpp
  gui/ImGuiLayer.h
  gui/ImGuiLayer.cpp
)

target_include_directories(lex-platform-windows PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-platform-windows PUBLIC lex-core lex-imgui lex-minhook)
target_link_libraries(lex-platform-windows PRIVATE
  user32 gdi32 ws2_32 psapi winmm ole32 windowscodecs dwmapi d3d11 d3d12 d3dcompiler dxgi dxguid
)
