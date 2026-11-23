// LEX CLIENT — Tests unitarios del Overlay Engine (C++).
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>

#include "bridge.h"

#define TEST(name) static void name()
#define EXPECT_TRUE(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #c); g_failures++; } } while(0)

static int g_failures = 0;

TEST(test_trim_and_readoverlaystate) {
  std::string s;
  const char* content = "{\"visible\": true, \"clickThrough\": false, \"fpsCap\": 90, \"positionX\": -1, \"opacity\": 0.92}";
  auto st = lex::engine::bridge::readOverlayStateFromString(content);
  EXPECT_TRUE(st.visible);
  EXPECT_TRUE(!st.clickThrough);
  EXPECT_TRUE(st.fpsCap == 90);
  EXPECT_TRUE(st.positionX == -1);
  EXPECT_TRUE(st.opacity > 0.91f);
}

TEST(test_readmodulestates_from_string) {
  std::string data = "{\n\"hud-fps\": {\"enabled\": true, \"updatedAt\": \"2026-11-23\"},\n\"hud-cps\": {\"enabled\": false, \"updatedAt\": \"2026-11-23\"}\n}";
  auto states = lex::engine::bridge::readModuleStatesFromString(data);
  EXPECT_TRUE(states.size() >= 2);
}

int main() {
  test_trim_and_readoverlaystate();
  test_readmodulestates_from_string();
  if (g_failures == 0) std::printf("[lex-overlay-unit-tests] todos pasaron\n");
  else std::printf("[lex-overlay-unit-tests] %d fallos\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
