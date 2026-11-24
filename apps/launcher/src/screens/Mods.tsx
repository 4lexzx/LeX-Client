// LEX CLIENT — Mod Menu con tarjetas ilustradas.
// Diseño inspirado en los menús de mods de Minecraft Java (Lunar / Badlion / LabyMod / Flarial)
// con familia gráfica e ilustraciones originales de LEX CLIENT.
// Los interruptores escriben en el sistema real de módulos vía IPC (modules:setState).

import React from 'react';
import type { ModuleMeta } from '@lex/shared';
import { ModuleIcon } from '../components/ModuleArt';
import { UiIcon, type IconName } from '../components/InterfaceIcons';

type UiCategory = 'all' | 'hud' | 'pvp' | 'visual' | 'performance' | 'utility' | 'cosmetics';
type StatusFilter = 'all' | 'enabled' | 'disabled' | 'favorites';
type SourceFilter = 'all' | ModuleMeta['dataSource'];
type ViewMode = 'grid' | 'compact';

interface CategoryDef {
  id: UiCategory;
  label: string;
  icon: IconName;
  blurb: string;
}

const CATEGORIES: CategoryDef[] = [
  { id: 'all', label: 'All Mods', icon: 'all', blurb: 'Todos los módulos del catálogo de LEX CLIENT.' },
  { id: 'hud', label: 'HUD', icon: 'hud', blurb: 'Información superpuesta: FPS, CPS, teclas, reloj y más.' },
  { id: 'pvp', label: 'PvP', icon: 'pvp', blurb: 'Módulos útiles en combate: CPS, keystrokes, mira y combate.' },
  { id: 'visual', label: 'Visual', icon: 'visual', blurb: 'Efectos y apariencia: crosshair, zoom, niebla y claridad.' },
  { id: 'performance', label: 'Performance', icon: 'performance', blurb: 'Rendimiento y recursos: FPS, memoria, TPS y sesión.' },
  { id: 'utility', label: 'Utility', icon: 'utility', blurb: 'Herramientas del cliente: atajos, capturas y estadísticas.' },
  { id: 'cosmetics', label: 'Cosmetics', icon: 'cosmetics', blurb: 'Personalización visual del cliente y del entorno de juego.' },
];

// Los módulos pueden aparecer en más de una categoría de la UI, como en los clientes Java.
// La categoría canónica del backend (ModuleMeta.category) se muestra siempre en la tarjeta.
const PVP_IDS = new Set(['hud-cps', 'hud-keystrokes', 'visual-crosshair', 'game-drift', 'game-haste']);
const PERFORMANCE_IDS = new Set(['hud-fps', 'hud-memory', 'game-tps', 'util-session']);
const COSMETICS_IDS = new Set(['visual-colors', 'visual-fog', 'visual-motionblur']);

function inCategory(m: ModuleMeta, category: UiCategory): boolean {
  switch (category) {
    case 'all': return true;
    case 'hud': return m.category === 'hud';
    case 'pvp': return PVP_IDS.has(m.id);
    case 'visual': return m.category === 'visual';
    case 'performance': return PERFORMANCE_IDS.has(m.id);
    case 'utility': return m.category === 'utilities';
    case 'cosmetics': return COSMETICS_IDS.has(m.id);
  }
}

function accentCategory(m: ModuleMeta, viewCategory: UiCategory): string {
  if (viewCategory !== 'all') return viewCategory;
  if (m.category === 'gameplay') return 'pvp';
  if (m.category === 'utilities') return 'utility';
  return m.category;
}

const BASE_CATEGORY_LABEL: Record<ModuleMeta['category'], string> = {
  hud: 'HUD',
  visual: 'Visual',
  gameplay: 'Gameplay',
  utilities: 'Utility',
};

const SOURCE_LABELS: Record<ModuleMeta['dataSource'], string> = {
  'external-overlay': 'Overlay externo',
  'game-memory-v1': 'Memoria v1',
  'game-memory-v2': 'Memoria v2',
  'script-api-v1': 'Script API',
  'local-only': 'Propio',
  reserved: 'Reservado',
};

const SOURCE_NOTES: Record<ModuleMeta['dataSource'], string> = {
  'external-overlay': 'Los datos los emite el overlay nativo (ventana GDI externa sobre el juego). No se inyecta nada en Minecraft.',
  'game-memory-v1': 'Adaptador de memoria del juego v1. Pendiente de validación con versiones reales (Fase 5).',
  'game-memory-v2': 'Adaptador de memoria del juego v2 (planificado).',
  'script-api-v1': 'Sólo Script API. Requiere respuesta del servidor; pendiente de Fase 5.',
  'local-only': 'Datos del propio cliente/overlay: reloj, sesión y atajos fijos del menú.',
  reserved: 'Esperando fuente nativa fiable. El módulo no inventa datos: se activará cuando exista un adaptador validado.',
};

// Textos cortos para la tarjeta; la descripción completa del catálogo se conserva en Opciones.
const SHORT_DESCRIPTIONS: Record<string, string> = {
  'hud-fps': 'FPS del overlay (no del juego).',
  'hud-cps': 'Clics por segundo con captura de bajo nivel.',
  'hud-keystrokes': 'Teclas W A S D en tiempo real.',
  'hud-clock': 'Reloj de pared con formato 24 h.',
  'hud-ping': 'Latencia del servidor (fuente pendiente).',
  'hud-memory': 'Uso de RAM del sistema en tiempo real (overlay).',
  'hud-direction': 'Brújula N / S / E / W del jugador.',
  'visual-crosshair': 'Mira cian dibujada sobre el juego.',
  'visual-motionblur': 'Reservado: sin acceso al render del juego.',
  'visual-fog': 'Reservado: sin fuente validada.',
  'visual-colors': 'Reservado: fuera del overlay externo.',
  'visual-zoom': 'Reservado: depende del FOV del juego.',
  'game-drift': 'Contador de muertes de la sesión.',
  'game-tps': 'Ticks por segundo del servidor.',
  'game-haste': 'Velocidad de movimiento del jugador.',
  'util-keybinds': 'ClickGUI con F8 para abrir; dentro del menú todo se controla con clic.',
  'util-session': 'Estadísticas locales de la sesión.',
  'util-screens': 'Reservado: capturas no implementadas.',
};

const FAVORITES_KEY = 'lex-client:mod-favorites';

function readFavorites(): string[] {
  try {
    const parsed: unknown = JSON.parse(window.localStorage.getItem(FAVORITES_KEY) ?? '[]');
    return Array.isArray(parsed) ? parsed.filter((v): v is string => typeof v === 'string') : [];
  } catch {
    return [];
  }
}

function ModSwitch({ on, onChange, label }: { on: boolean; onChange: (v: boolean) => void; label: string }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={on}
      aria-label={label}
      data-on={on}
      className="mc-switch"
      onClick={(e) => { e.stopPropagation(); onChange(!on); }}
    >
      <span className="knob" />
      <span className="switch-label">{on ? 'ON' : 'OFF'}</span>
    </button>
  );
}

interface CardProps {
  module: ModuleMeta;
  index: number;
  enabled: boolean;
  favorite: boolean;
  compact: boolean;
  viewCategory: UiCategory;
  onToggle: (id: string, enabled: boolean) => void;
  onFavorite: (id: string) => void;
  onSettings: (m: ModuleMeta) => void;
}

function ModCard({ module: m, index, enabled, favorite, compact, viewCategory, onToggle, onFavorite, onSettings }: CardProps) {
  const sourceTone = m.dataSource === 'reserved' ? 'warn' : m.dataSource === 'external-overlay' || m.dataSource === 'local-only' ? 'ok' : 'off';
  return (
    <article
      className={`mod-card ${compact ? 'compact' : ''} ${enabled ? 'is-on' : ''}`}
      data-cat={accentCategory(m, viewCategory)}
      style={{ animationDelay: `${Math.min(index, 18) * 28}ms` }}
    >
      <div className="mod-art">
        <div className="mod-art-aura" aria-hidden="true" />
        <span className="mod-art-index">{String(index + 1).padStart(2, '0')}</span>
        <span className="mod-icon-frame"><ModuleIcon id={m.id} /></span>
        {m.dataSource === 'reserved' && !compact && <span className="mod-art-tag">PRÓXIMAMENTE</span>}
        <button
          type="button"
          className={`mod-fav ${favorite ? 'on' : ''}`}
          aria-label={favorite ? 'Quitar de favoritos' : 'Añadir a favoritos'}
          aria-pressed={favorite}
          title={favorite ? 'Quitar de favoritos' : 'Añadir a favoritos'}
          onClick={() => onFavorite(m.id)}
        >
          <UiIcon name="star" size={14} />
        </button>
      </div>
      <div className="mod-body">
        <div className="mod-title-row">
          <h3 className="mod-name" title={m.name}>{m.name}</h3>
          <button
            type="button"
            className="mod-gear"
            aria-label={`Opciones de ${m.name}`}
            title="Opciones"
            onClick={() => onSettings(m)}
          >
            <UiIcon name="gear" size={15} />
          </button>
        </div>
        <p className="mod-desc">{SHORT_DESCRIPTIONS[m.id] ?? m.description}</p>
      </div>
      <div className="mod-foot">
        <span className="mod-foot-tag">{BASE_CATEGORY_LABEL[m.category]}</span>
        <span className={`mod-source-dot ${sourceTone}`} title={SOURCE_LABELS[m.dataSource]} />
        <span className="mod-foot-spacer" />
        <ModSwitch on={enabled} onChange={(v) => onToggle(m.id, v)} label={`Activar ${m.name}`} />
      </div>
    </article>
  );
}

interface ModalProps {
  module: ModuleMeta;
  enabled: boolean;
  favorite: boolean;
  onToggle: (id: string, enabled: boolean) => void;
  onFavorite: (id: string) => void;
  onClose: () => void;
}

function ModuleSettingsModal({ module: m, enabled, favorite, onToggle, onFavorite, onClose }: ModalProps) {
  const copyId = () => {
    try { void window.navigator.clipboard?.writeText(m.id); } catch { /* clipboard no disponible */ }
  };
  return (
    <div className="mods-modal-backdrop" role="presentation" onMouseDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="mods-modal" data-cat={m.category === 'gameplay' ? 'pvp' : m.category === 'utilities' ? 'utility' : m.category} role="dialog" aria-modal="true" aria-label={`Opciones de ${m.name}`}>
        <div className="mods-modal-head">
          <div className="mods-modal-thumb"><ModuleIcon id={m.id} /></div>
          <div>
            <h2>{m.name}</h2>
            <div className="modal-sub">#{m.id} · {SOURCE_LABELS[m.dataSource]}</div>
          </div>
          <button type="button" className="mods-modal-close" aria-label="Cerrar" onClick={onClose}><UiIcon name="close" size={14} /></button>
        </div>
        <div className="mods-modal-body">
          <section className="mods-modal-section">
            <h4>Descripción</h4>
            <p>{m.description}</p>
          </section>
          <section className="mods-modal-section">
            <h4>Ficha del módulo</h4>
            <div className="mods-meta">
              <div className="mods-meta-row"><span>Categoría</span><b>{BASE_CATEGORY_LABEL[m.category]}</b></div>
              <div className="mods-meta-row"><span>Fuente de datos</span><b>{SOURCE_LABELS[m.dataSource]}</b></div>
              <div className="mods-meta-row"><span>Renderer</span><b>{m.overlayRenderer ?? '—'}</b></div>
              <div className="mods-meta-row"><span>Versión mínima</span><b>{m.requiresMinVersion ?? '—'}</b></div>
              <div className="mods-meta-row"><span>Atajos</span><b>{m.keybinds?.join(' · ') ?? '—'}</b></div>
              <div className="mods-meta-row"><span>Identificador</span><b>{m.id}</b></div>
            </div>
          </section>
          <div className="mods-modal-note"><UiIcon name="info" size={15} /><span>{SOURCE_NOTES[m.dataSource]}</span></div>
          <div className="mods-modal-actions">
            <button type="button" className="mods-mini-btn" onClick={() => onFavorite(m.id)}>
              <UiIcon name="star" size={13} /> {favorite ? 'Quitar favorito' : 'Favorito'}
            </button>
            <button type="button" className="mods-mini-btn" onClick={copyId}><UiIcon name="copy" size={13} /> Copiar ID</button>
            <ModSwitch on={enabled} onChange={(v) => onToggle(m.id, v)} label={`Activar ${m.name}`} />
          </div>
        </div>
      </div>
    </div>
  );
}

export function Mods() {
  const [catalog, setCatalog] = React.useState<ModuleMeta[]>([]);
  const [loading, setLoading] = React.useState(true);
  const [enabledStates, setEnabledStates] = React.useState<Record<string, boolean>>({});
  const [query, setQuery] = React.useState('');
  const [category, setCategory] = React.useState<UiCategory>('all');
  const [status, setStatus] = React.useState<StatusFilter>('all');
  const [source, setSource] = React.useState<SourceFilter>('all');
  const [view, setView] = React.useState<ViewMode>('grid');
  const [favorites, setFavorites] = React.useState<string[]>(readFavorites);
  const [selected, setSelected] = React.useState<ModuleMeta | null>(null);
  const [actionError, setActionError] = React.useState<string | null>(null);

  React.useEffect(() => {
    let cancelled = false;
    window.lex.ipc.catalog().then((r) => {
      if (!cancelled) setCatalog(r.modules ?? []);
    }).catch((e) => {
      console.error('[LEX] catalog error:', e);
      if (!cancelled) setActionError('No se pudo cargar el catálogo de módulos.');
    }).finally(() => { if (!cancelled) setLoading(false); });
    return () => { cancelled = true; };
  }, []);

  React.useEffect(() => {
    let cancelled = false;
    window.lex.ipc.modulesGet().then((m) => {
      if (!cancelled) setEnabledStates(Object.fromEntries(Object.entries(m).map(([k, v]) => [k, !!v.enabled])));
    }).catch(() => { /* primer arranque: sin estado guardado */ });
    return () => { cancelled = true; };
  }, []);

  React.useEffect(() => {
    try { window.localStorage.setItem(FAVORITES_KEY, JSON.stringify(favorites)); } catch { /* almacenamiento no disponible */ }
  }, [favorites]);

  React.useEffect(() => {
    if (!selected) return;
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') setSelected(null); };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [selected]);

  async function toggleModule(id: string, enabled: boolean) {
    setActionError(null);
    setEnabledStates((prev) => ({ ...prev, [id]: enabled }));
    try {
      await window.lex.ipc.modulesSetState({ id, enabled });
    } catch (e) {
      setEnabledStates((prev) => ({ ...prev, [id]: !enabled }));
      setActionError('No se pudo guardar el estado del módulo.');
      console.error('[LEX] modulesSetState error:', e);
    }
  }

  function toggleFavorite(id: string) {
    setFavorites((prev) => (prev.includes(id) ? prev.filter((x) => x !== id) : [...prev, id]));
  }

  const activeCount = catalog.filter((m) => !!enabledStates[m.id]).length;
  const countIn = (c: UiCategory) => catalog.filter((m) => inCategory(m, c)).length;
  const currentBlurb = CATEGORIES.find((c) => c.id === category)?.blurb ?? '';

  const filtered = React.useMemo(() => {
    const needle = query.trim().toLowerCase();
    return catalog.filter((m) => {
      if (!inCategory(m, category)) return false;
      if (needle && !`${m.name} ${m.description} ${m.id}`.toLowerCase().includes(needle)) return false;
      if (source !== 'all' && m.dataSource !== source) return false;
      if (status === 'enabled' && !enabledStates[m.id]) return false;
      if (status === 'disabled' && enabledStates[m.id]) return false;
      if (status === 'favorites' && !favorites.includes(m.id)) return false;
      return true;
    });
  }, [catalog, category, query, source, status, enabledStates, favorites]);

  const statusChips: Array<{ id: StatusFilter; label: string; icon?: IconName }> = [
    { id: 'all', label: 'Todos' },
    { id: 'enabled', label: 'Activados' },
    { id: 'disabled', label: 'Desactivados' },
    { id: 'favorites', label: 'Favoritos', icon: 'star' },
  ];

  return (
    <div className="mods-shell">
      <header className="mods-head animate-slideUp">
        <div>
          <div className="mods-kicker">Mod Menu · Lex Client</div>
          <h1 className="mods-title">MODS</h1>
          <p className="mods-subtitle">
            Módulos ilustrados con el espíritu de los clientes de Minecraft Java. Cada tarjeta refleja el estado real
            del módulo y su fuente de datos: nunca se inventan valores del juego.
          </p>
        </div>
        <div className="mods-active-pill" title="Módulos activos sobre el total del catálogo">
          <span className="status-dot ok" />
          <div>
            <strong>{activeCount}</strong>
            <span> / {catalog.length} activos</span>
          </div>
          <div className="mods-active-track"><i style={{ width: `${catalog.length ? (activeCount / catalog.length) * 100 : 0}%` }} /></div>
        </div>
      </header>

      <div className="mods-toolbar animate-slideUp">
        <div className="mods-search">
          <UiIcon name="search" size={16} />
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Buscar módulos por nombre, descripción o ID…"
            aria-label="Buscar módulos"
          />
          {query && (
            <button type="button" aria-label="Limpiar búsqueda" onClick={() => setQuery('')}><UiIcon name="close" size={12} /></button>
          )}
        </div>
        <label>
          <span className="sr-only" style={{ position: 'absolute', width: 1, height: 1, overflow: 'hidden', clip: 'rect(0,0,0,0)' }}>Filtrar por fuente</span>
          <select className="mods-select" value={source} onChange={(e) => setSource(e.target.value as SourceFilter)}>
            <option value="all">Todas las fuentes</option>
            {(Object.keys(SOURCE_LABELS) as ModuleMeta['dataSource'][]).map((s) => (
              <option key={s} value={s}>{SOURCE_LABELS[s]}</option>
            ))}
          </select>
        </label>
        <div className="mods-viewtoggle" role="group" aria-label="Modo de vista">
          <button type="button" className={view === 'grid' ? 'active' : ''} aria-label="Vista de tarjetas" aria-pressed={view === 'grid'} title="Vista de tarjetas" onClick={() => setView('grid')}><UiIcon name="grid" size={15} /></button>
          <button type="button" className={view === 'compact' ? 'active' : ''} aria-label="Vista compacta" aria-pressed={view === 'compact'} title="Vista compacta" onClick={() => setView('compact')}><UiIcon name="list" size={15} /></button>
        </div>
      </div>

      <div className="mods-filters">
        <UiIcon name="filter" size={14} />
        {statusChips.map((chip) => (
          <button
            key={chip.id}
            type="button"
            className={`mod-chip ${status === chip.id ? 'active' : ''}`}
            aria-pressed={status === chip.id}
            onClick={() => setStatus(chip.id)}
          >
            {chip.icon && <UiIcon name={chip.icon} size={11} />}
            {chip.label}
            {chip.id === 'favorites' && favorites.length > 0 && <i>{favorites.length}</i>}
          </button>
        ))}
        {actionError && <span style={{ fontSize: 11, color: '#ff8894', fontWeight: 600 }}>{actionError}</span>}
        <span className="mods-blurb">{currentBlurb}</span>
        <span className="mods-result-count">{filtered.length} módulos</span>
      </div>

      <div className="mods-layout">
        <aside className="mods-rail" aria-label="Categorías de módulos">
          <div className="mods-rail-label">Categorías</div>
          {CATEGORIES.map((c) => (
            <button
              key={c.id}
              type="button"
              data-cat={c.id}
              className={`mod-cat-btn ${category === c.id ? 'active' : ''}`}
              aria-pressed={category === c.id}
              title={c.blurb}
              onClick={() => setCategory(c.id)}
            >
              <span className="cat-icon"><UiIcon name={c.icon} size={15} /></span>
              <span>{c.label}</span>
              <span className="cat-count">{countIn(c.id)}</span>
            </button>
          ))}
          <div className="mods-rail-note">
            <strong>Fuente honesta.</strong> Los módulos marcados como reservado esperan un adaptador validado;
            nunca muestran datos simulados.
          </div>
        </aside>

        <section className={`mods-grid ${view === 'compact' ? 'compact' : ''}`} aria-label="Lista de módulos">
          {loading && catalog.length === 0 ? (
            <div className="mods-empty"><div className="empty-icon"><UiIcon name="spark" size={20} /></div><h3>Cargando catálogo…</h3></div>
          ) : filtered.length === 0 ? (
            <div className="mods-empty">
              <div className="empty-icon"><UiIcon name={status === 'favorites' ? 'star' : 'search'} size={20} /></div>
              <h3>{status === 'favorites' ? 'Aún no hay favoritos' : 'No hay módulos que coincidan'}</h3>
              <p>
                {status === 'favorites'
                  ? 'Pulsa la estrella de una tarjeta para guardar aquí tus módulos preferidos.'
                  : 'Prueba con otra búsqueda, cambia la fuente o selecciona otra categoría.'}
              </p>
            </div>
          ) : (
            filtered.map((m, i) => (
              <ModCard
                key={m.id}
                module={m}
                index={i}
                enabled={!!enabledStates[m.id]}
                favorite={favorites.includes(m.id)}
                compact={view === 'compact'}
                viewCategory={category}
                onToggle={toggleModule}
                onFavorite={toggleFavorite}
                onSettings={setSelected}
              />
            ))
          )}
        </section>
      </div>

      <footer className="mods-legend">
        <span className="dot" style={{ background: '#57c98a' }} /> Overlay externo / local (datos reales)
        <span className="dot" style={{ background: '#e0b052', marginLeft: 10 }} /> Reservado (pendiente de fuente)
        <span style={{ marginLeft: 'auto' }}>Los cambios se guardan en %APPDATA%\lex-client\modules.json</span>
      </footer>

      {selected && (
        <ModuleSettingsModal
          module={selected}
          enabled={!!enabledStates[selected.id]}
          favorite={favorites.includes(selected.id)}
          onToggle={toggleModule}
          onFavorite={toggleFavorite}
          onClose={() => setSelected(null)}
        />
      )}
    </div>
  );
}
