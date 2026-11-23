import { useApp } from '../stores/appStore';
import type { MinecraftInstallation } from '@lex/shared';

export function useStore() {
  return useApp();
}

export function useInstallations(): MinecraftInstallation[] {
  return useApp().installations;
}
