import { Brand } from '@lex/ui-components';
import { NavItem } from './NavItem';
import { useApp } from '../stores/appStore';
import type { Screen } from '../stores/appStore';
import type { IconName } from './InterfaceIcons';

const ITEMS: Array<{ id: Screen; label: string; icon: IconName }> = [
  { id: 'home', label: 'Inicio', icon: 'home' },
  { id: 'versions', label: 'Versiones', icon: 'box' },
  { id: 'mods', label: 'Módulos', icon: 'grid' },
  { id: 'cosmetics', label: 'Cosméticos', icon: 'cosmetics' },
  { id: 'settings', label: 'Ajustes', icon: 'gear' },
];

export function Sidebar({ current, onNav }: { current: Screen; onNav: (s: Screen) => void }) {
  const { status } = useApp();
  return (
    <aside className="w-56 flex-none flex flex-col p-3 border-r border-[var(--border)] bg-[var(--bg-secondary)]">
      <div className="pt-6 pb-4 px-2">
        <Brand size="sm" />
      </div>
      <nav className="flex-1 space-y-1">
        {ITEMS.map((item) => (
          <NavItem key={item.id} active={current === item.id} label={item.label} icon={item.icon} onClick={() => onNav(item.id)} />
        ))}
      </nav>
      {status && (
        <div className="mt-4 mx-2 px-3 py-2 rounded-lg bg-[var(--surface)] text-[11px] text-[var(--text-secondary)]">
          {status.text}
        </div>
      )}
    </aside>
  );
}
