#include "core/settings/SettingVisitor.h"

namespace lex::core::settings::detail {

void dispatchVisit(SettingVisitor& visitor, SettingBase& setting) {
  switch (setting.type()) {
    case SettingType::Bool:
      visitor.visit(static_cast<BoolSetting&>(setting));
      break;
    case SettingType::Int:
      visitor.visit(static_cast<IntSetting&>(setting));
      break;
    case SettingType::Float:
      visitor.visit(static_cast<FloatSetting&>(setting));
      break;
    case SettingType::Color:
      visitor.visit(static_cast<ColorSetting&>(setting));
      break;
    case SettingType::Enum:
      visitor.visit(static_cast<EnumSetting&>(setting));
      break;
    case SettingType::Keybind:
      visitor.visit(static_cast<KeybindSetting&>(setting));
      break;
  }
}

}  // namespace lex::core::settings::detail
