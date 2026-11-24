#pragma once

#include "core/settings/Setting.h"

namespace lex::core::settings {

// Implementado por el ClickGUI en Fase 2 para dibujar cada control segun tipo.
class SettingVisitor {
 public:
  virtual ~SettingVisitor() = default;

  virtual void visit(BoolSetting& setting) = 0;
  virtual void visit(IntSetting& setting) = 0;
  virtual void visit(FloatSetting& setting) = 0;
  virtual void visit(ColorSetting& setting) = 0;
  virtual void visit(EnumSetting& setting) = 0;
  virtual void visit(KeybindSetting& setting) = 0;
};

}  // namespace lex::core::settings
