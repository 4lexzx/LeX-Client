// LEX CLIENT — MóduloUtilidades: Keybind Manager.
// FUENTE: local-only — gestión de atajos de teclado del cliente (datos locales; visibles para el usuario).
import type { Module, ModuleMeta } from '@lex/module-sdk';

export const meta: ModuleMeta = {
  id: 'util-keybinds',
  name: 'Keybind Manager',
  description: 'Registro y gestión de atajos de teclado de cliente. Datos locales: gestión de binds y aliases de cliente.',
  category: 'utilities',
  dataSource: 'local-only',
};

export interface Keybind {
  action: string;
  key: string;
  enabled: boolean;
}

const registered: Keybind[] = [];

export function registerKeybind(action: string, key: string): boolean {
  if (registered.some((k) => k.action === action)) {
    const existing = registered.find((k) => k.action === action);
    if (existing) existing.key = key;
    return true;
  }
  registered.push({ action, key, enabled: true });
  return true;
}

export function listKeybinds(): readonly Keybind[] {
  return [...registered];
}

export const keybindsModule: Module = {
  meta,
  activate() {
    // Nothing — los binds ya están registrados por default.
  },
  deactivate() {
    // Nothing.
  },
};
