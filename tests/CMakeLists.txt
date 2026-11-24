add_executable(lex-core-tests
  test_main.cpp
  NullPlatform.h
  NullPlatform.cpp
)

target_include_directories(lex-core-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-core-tests PRIVATE lex-core lex-modules-objects)

add_test(NAME lex-core-tests COMMAND lex-core-tests)

add_executable(lex-hook-tests hook_tests_main.cpp NullPlatform.cpp)
target_include_directories(lex-hook-tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(lex-hook-tests PRIVATE lex-core lex-platform-windows)
add_test(NAME lex-hook-tests COMMAND lex-hook-tests)
