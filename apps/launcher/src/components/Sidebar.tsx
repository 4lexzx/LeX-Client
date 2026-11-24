import React from 'react';
import logoUrl from '../../assets/lex-logo.png';
import { UiIcon } from './InterfaceIcons';
import { useApp } from '../stores/appStore';
import type { Screen } from '../stores/appStore';
import type { IconName } from './InterfaceIcons';

const ITEMS: Array<{ id: Screen; label: string; icon: IconName }> = [
  { id: 'home', label: 'Inicio', icon: 'play' },
  { id: 'versions', label: 'Versiones', icon: 'box' },
  { id: 'mods', label: 'Módulos', icon: 'grid' },
  { id: 'cosmetics', label: 'Cosméticos', icon: 'cosmetics' },
  { id: 'settings', label: 'Ajustes', icon: 'gear' },
];

export function Sidebar({ current, onNav }: { current: Screen; onNav: (s: Screen) => void }) {
  const { status } = useApp();
  const [logo, setLogo] = React.useState<string | null>(null);

  React.useEffect(() => {
    window.lex?.ipc?.logoGet?.().then((r) => { if (r?.dataUrl) setLogo(r.dataUrl); }).catch(() => {});
    const off = window.lex?.on?.('lex:logo-updated', () => {
      window.lex?.ipc?.logoGet?.().then((r) => { if (r?.dataUrl) setLogo(r.dataUrl); }).catch(() => {});
    });
    return () => off?.();
  }, []);

  return (
    <aside className="lex-sidebar">
      <div className="lex-rail-logo" title="LEX CLIENT">
        <img src={logo ?? logoUrl} alt="LEX CLIENT" />
      </div>

      <nav className="lex-nav" aria-label="Navegación principal">
        {ITEMS.map((item) => (
          <button
            key={item.id}
            type="button"
            title={item.label}
            aria-label={item.label}
            className={`lex-nav-item${current === item.id ? ' is-active' : ''}`}
            onClick={() => onNav(item.id)}
          >
            <span className="lex-nav-icon"><UiIcon name={item.icon} size={20} /></span>
          </button>
        ))}
      </nav>

      <div className="lex-side-foot">
        <div className={`lex-rail-status tone-${status?.tone ?? 'ok'}`} title={status?.text ?? 'Sistema listo'}>
          <UiIcon name="pulse" size={16} />
        </div>
        <div className="lex-rail-key" title="F8 abre el menú en el juego">F8</div>
      </div>
    </aside>
  );
}
