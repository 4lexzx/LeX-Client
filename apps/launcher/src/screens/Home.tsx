import { UiIcon } from '../components/InterfaceIcons';
import { PlayButton } from '../components/PlayButton';
import { useApp, useAppDispatch } from '../stores/appStore';
import { useDetectOnMount } from '../hooks/useDetectOnMount';

const SOCIAL: Array<'star' | 'grid' | 'spark' | 'layers' | 'folder'> = [
  'star', 'grid', 'spark', 'layers', 'folder',
];

const NEWS = [
  { tag: 'CLICKGUI', title: 'Menú F8 rediseñado', desc: 'Tarjetas 3D, iconos grandes y ajustes por módulo.', icon: 'grid' as const },
  { tag: 'OVERLAY', title: 'HUD sin contorno', desc: 'Keystrokes, armadura y contadores con estilo Flarial.', icon: 'hud' as const },
  { tag: 'RENDIMIENTO', title: 'FPS reales', desc: 'Puente DX11/DX12 y CPS 1:1 desde el hook global.', icon: 'pulse' as const },
  { tag: 'SEGURIDAD', title: 'Sin cuentas', desc: 'Sin auth, sin credenciales y sin archivos del juego.', icon: 'shield' as const },
];

export function Home() {
  const { installations, latest, loading, status } = useApp();
  const dispatch = useAppDispatch();
  useDetectOnMount();

  const isLoading = loading.detect;
  const version = latest?.version ?? '—';

  return (
    <div className="lx-home animate-slideUp">
      <header className="lx-topbar">
        <div className="lx-social">
          {SOCIAL.map((icon, i) => <span key={i}><UiIcon name={icon} size={15} /></span>)}
        </div>
        <div className="lx-toppill"><UiIcon name="pulse" size={13} /> {isLoading ? 'DETECTANDO…' : `${installations.length} instalaciones`}</div>
        <div className="lx-toppill lx-toppill-accent"><UiIcon name="star" size={13} /> SIN AUTH</div>
        <div className="lx-topbar-spacer" />
        <div className="lx-topicon"><UiIcon name="grid" size={17} /><b>3</b></div>
        <div className="lx-topicon"><UiIcon name="spark" size={17} /><b>2</b></div>
        <div className="lx-window-dots"><span /><span /></div>
      </header>

      <div className="lx-content">
        <section className="lx-hero">
          <div className="lx-hero-copy">
            <div className="lx-hero-badge">LEX CLIENT · BEDROCK WINDOWS</div>
            <h1>Minecraft {version.replace(/^[^-]*-/, '') || '1.20.1'}</h1>
            <p>
              El cliente se acopla a tu juego con un overlay externo: F8 abre el menú,
              el ratón manda y los módulos se ajustan uno por uno. Sin cuentas, sin
              archivos del juego y sin atajos ocultos.
            </p>
            <div className="lx-launch">
              <PlayButton installation={latest ?? installations[0] ?? null} disabled={isLoading} />
              <button
                type="button"
                className="lx-chip"
                onClick={() => dispatch({ type: 'set-current', screen: 'mods' })}
              >
                <UiIcon name="grid" size={14} /> VER MÓDULOS
              </button>
            </div>
            <div className="lx-status-line"><span className="lx-dot" /> {status?.text ?? 'Sistema listo.'}</div>
          </div>

          <aside className="lx-account">
            <div className="lx-avatar"><UiIcon name="shield" size={54} /></div>
            <div className="lx-avatar-name">PLAYER</div>
            <div className="lx-avatar-sub">Minecraft for Windows · local</div>
            <button type="button" className="lx-account-btn" onClick={() => dispatch({ type: 'set-current', screen: 'settings' })}>
              ADMINISTRAR
            </button>
            <button type="button" className="lx-account-btn lx-account-ghost" onClick={() => dispatch({ type: 'set-current', screen: 'versions' })}>
              VERSIONES
            </button>
          </aside>
        </section>

        <section className="lx-news">
          {NEWS.map((n) => (
            <button key={n.tag} type="button" className="lx-news-card" onClick={() => dispatch({ type: 'set-current', screen: n.tag === 'CLICKGUI' ? 'mods' : 'settings' })}>
              <span className="lx-news-art"><UiIcon name={n.icon} size={34} /></span>
              <span className="lx-news-tag">{n.tag}</span>
              <b>{n.title}</b>
              <small>{n.desc}</small>
            </button>
          ))}
        </section>
      </div>
    </div>
  );
}
