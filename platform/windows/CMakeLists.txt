add_library(lex-platform-windows STATIC
  Exports.h
  WindowsPlatform.h
  WindowsPlatform.cpp
)

target_include_directories(lex-platform-windows PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-platform-windows PUBLIC lex-core)
target_link_libraries(lex-platform-windows PRIVATE user32)
