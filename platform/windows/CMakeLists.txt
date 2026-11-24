add_library(lex-platform-windows STATIC
  Exports.h
  WindowsPlatform.h
  WindowsPlatform.cpp
  hooks/HookSink.h
  hooks/VtableHook.h
  hooks/VtableHook.cpp
  hooks/DxgiHook.h
  hooks/DxgiHook.cpp
  gui/VectorIcons.h
  gui/VectorIcons.cpp
  gui/IconAtlas.h
  gui/IconAtlas.cpp
  resources/IconResource.h
  gui/SettingEditor.h
  gui/SettingEditor.cpp
  gui/ClickGuiRenderer.h
  gui/ClickGuiRenderer.cpp
  gui/ImGuiLayer.h
  gui/ImGuiLayer.cpp
)

target_include_directories(lex-platform-windows PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-platform-windows PUBLIC lex-core lex-imgui)
target_link_libraries(lex-platform-windows PRIVATE
  user32 dwmapi d3d11 d3d12 d3dcompiler dxgi dxguid
)
