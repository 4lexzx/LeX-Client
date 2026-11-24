# LEX CLIENT — Overlay Engine (C++ Win32 + Direct2D). Compilado con MSVC x64 o MinGW-w64 (WinLibs).
cmake_minimum_required(VERSION 3.25)
project(lex-overlay-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Leemos compiladores desde entorno MinGW si existen (detectados con Get-Command o PATH).
if (NOT MSVC)
  set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
  set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
endif()

if (MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /MP)
  add_link_options(/SUBSYSTEM:WINDOWS)
else()
  add_link_options(-mwindows)
endif()

add_definitions(-DWIN32 -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

set(LEX_LIBS user32 gdi32 kernel32 advapi32 shell32 d2d1 dwrite dwmapi msimg32)

add_executable(lex-overlay-engine
  src/main.cpp
  src/engine.cpp
  src/hooks.cpp
  src/bridge.cpp
  src/inject.cpp
)
target_link_libraries(lex-overlay-engine PRIVATE ${LEX_LIBS})

add_executable(lex-overlay-unit-tests
  src/unit_tests.cpp
  src/engine.cpp
  src/hooks.cpp
  src/bridge.cpp
  src/inject.cpp
)
target_link_libraries(lex-overlay-unit-tests PRIVATE user32 gdi32 kernel32 advapi32 msimg32)
