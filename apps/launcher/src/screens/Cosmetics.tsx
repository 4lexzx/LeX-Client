// LEX CLIENT — Galería visual de cosméticos (atlas LEXTEX1 reales).
// La vista previa y las miniaturas reproducen los frames del atlas; activar
// escribe en el mismo sistema de módulos que consume el overlay in-game.

import React from 'react';
import { Button } from '@lex/ui-components';
import { UiIcon } from '../components/InterfaceIcons';
import { useDetectOnMount } from '../hooks/useDetectOnMount';
import { makeGallery, type Gallery, type Cosmetic, type ImportResult } from '@lex/cosmetics';
import { useAppHelpers } from '../stores/appStore';

type PreviewProps = {
  cosmetic: Cosmetic | null;
  compact?: boolean;
};

/** Visor del atlas real del cosmético: reproduce los frames LEXTEX1 en el launcher. */
function AnimatedPreview({ cosmetic, compact = false }: PreviewProps) {
  const canvasRef = React.useRef<HTMLCanvasElement>(null);
  const [image, setImage] = React.useState<HTMLImageElement | null>(null);
  const frames = Math.max(1, cosmetic?.frames ?? 1);
  const frameMs = Math.max(16, cosmetic?.frameMs ?? 120);

  React.useEffect(() => {
    setImage(null);
    const file = cosmetic?.preview;
    if (!file) return;
    let cancelled = false;
    window.lex.ipc.cosmeticsPreview(file).then(({ dataUrl }) => {
      if (!dataUrl || cancelled) return;
      const img = new Image();
      img.onload = () => { if (!cancelled) setImage(img); };
      img.src = dataUrl;
    }).catch(() => {});
    return () => { cancelled = true; };
  }, [cosmetic?.preview]);

  React.useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let raf = 0;
    let frame = 0;
    let last = 0;

    const draw = (t: number) => {
      const w = canvas.clientWidth || (compact ? 180 : 480);
      const h = canvas.clientHeight || (compact ? 116 : 300);
      if (canvas.width !== w || canvas.height !== h) {
        canvas.width = w;
        canvas.height = h;
      }

      const bg = ctx.createLinearGradient(0, 0, w, h);
      bg.addColorStop(0, '#11141d');
      bg.addColorStop(0.55, '#0b0d13');
      bg.addColorStop(1, '#151a25');
      ctx.fillStyle = bg;
      ctx.fillRect(0, 0, w, h);

      ctx.strokeStyle = compact ? 'rgba(255,139,208,0.08)' : 'rgba(0,200,232,0.10)';
      ctx.lineWidth = 1;
      const step = compact ? 18 : 28;
      for (let x = 0; x < w; x += step) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); }
      for (let y = 0; y < h; y += step) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }

      if (!compact) {
        const glow = ctx.createRadialGradient(w / 2, h / 2, 4, w / 2, h / 2, Math.max(w, h) * 0.42);
        glow.addColorStop(0, 'rgba(255,139,208,0.10)');
        glow.addColorStop(1, 'rgba(255,139,208,0)');
        ctx.fillStyle = glow;
        ctx.fillRect(0, 0, w, h);
      }

      if (t - last >= frameMs) {
        last = t;
        frame = (frame + 1) % frames;
      }

      const id = cosmetic?.id ?? '';
      const kind = id.includes('wing') ? 'wings' : id.includes('armor') ? 'armor' : 'cape';
      const unit = compact ? 1 : 2;
      const px = (v: number) => v * unit;
      const drawPlayer = (cx: number, cy: number) => {
        const s = compact ? 0.45 : 1;
        const U = (v: number) => Math.round(v * s);
        const left = cx - U(18), top = cy - U(44);
        ctx.fillStyle = '#d8a06a'; ctx.fillRect(left, top, U(36), U(28)); // head
        ctx.fillStyle = '#3a2a22'; ctx.fillRect(left, top, U(36), U(6));  // hair
        ctx.fillStyle = '#2f6db5'; ctx.fillRect(left + U(4), top + U(29), U(28), U(24)); // torso
        ctx.fillStyle = '#d8a06a'; ctx.fillRect(left - U(8), top + U(29), U(8), U(22));
        ctx.fillRect(left + U(36), top + U(29), U(8), U(22));
        ctx.fillStyle = '#26324a'; ctx.fillRect(left + U(4), top + U(53), U(12), U(22));
        ctx.fillRect(left + U(20), top + U(53), U(12), U(22));
        ctx.fillStyle = '#1b2333';
        ctx.fillRect(left + U(7), top + U(11), U(5), U(5));
        ctx.fillRect(left + U(24), top + U(11), U(5), U(5));
      };
      const drawAtlas = (beforePlayer: boolean) => {
        if (!image || !image.complete || image.naturalWidth <= 0) return;
        const fw = image.naturalWidth / frames;
        const fh = image.naturalHeight;
        const maxW = w * (kind === 'armor' ? 0.34 : compact ? 0.64 : 0.52);
        const maxH = h * (kind === 'armor' ? 0.36 : compact ? 0.78 : 0.74);
        const scale = Math.min(maxW / fw, maxH / fh);
        const dw = fw * scale, dh = fh * scale;
        const dx = (w - dw) / 2;
        const dy = kind === 'armor'
          ? h * (compact ? 0.3 : 0.26)
          : (h - dh) / 2 - (compact ? 0 : 8);
        ctx.save();
        ctx.imageSmoothingEnabled = true;
        if (!beforePlayer && kind === 'armor') ctx.globalAlpha = 0.92;
        if (kind !== 'armor') {
          ctx.shadowColor = 'rgba(49,87,131,0.55)';
          ctx.shadowBlur = compact ? 5 : 16;
        }
        ctx.drawImage(image, frame * fw, 0, fw, fh, dx, dy, dw, dh);
        ctx.restore();
      };

      if (kind !== 'armor') drawAtlas(true);
      drawPlayer(w / 2, h * (compact ? 0.56 : 0.6));
      if (kind === 'armor') drawAtlas(false);
      if (!image && cosmetic) {
        // Sin atlas propio: maniquí + indicador honesto de efecto.
        ctx.fillStyle = 'rgba(49,87,131,0.75)';
        ctx.fillRect(w / 2 - px(12), h * 0.28, px(24), px(6));
      }

      ctx.globalAlpha = 1;
      if (compact) {
        ctx.font = '600 10px "JetBrains Mono", monospace';
        ctx.textAlign = 'left';
        ctx.fillStyle = 'rgba(255,139,208,0.9)';
        ctx.fillText(`F${frame + 1}/${frames}`, 8, h - 7);
      } else {
        ctx.textAlign = 'center';
        ctx.font = '700 11px "JetBrains Mono", monospace';
        ctx.fillStyle = '#ff8bd0';
        ctx.fillText(cosmetic ? `SKIN DE PRUEBA · ${kind.toUpperCase()} · FRAME ${frame + 1}/${frames}` : 'SIN COSMÉTICO SELECCIONADO', w / 2, h - 18);
        ctx.font = '11px Inter, sans-serif';
        ctx.fillStyle = '#9aa1b5';
        ctx.fillText(cosmetic ? cosmetic.name : 'Selecciona un cosmético de la galería', w / 2, h - 34);
      }
      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, [cosmetic, image, frames, frameMs, compact]);

  return (
    <div className={compact ? 'cosmetic-thumb' : 'cosmetic-stage'}>
      <canvas
        data-cosmetic-preview={compact ? undefined : '1'}
        ref={canvasRef}
        width={compact ? 180 : 480}
        height={compact ? 116 : 300}
        className="cosmetic-canvas"
      />
      {!compact && <span className="cosmetic-stage-badge">LEXTEX1 · LIVE</span>}
    </div>
  );
}

function Pill({ children, tone = 'neutral' }: { children: React.ReactNode; tone?: 'neutral' | 'pink' | 'green' | 'amber' }) {
  return <span className={`cosmetic-pill ${tone}`}>{children}</span>;
}

export function Cosmetics() {
  const { setStatus } = useAppHelpers();
  const [gallery, setGallery] = React.useState<Gallery>(() => makeGallery([]));
  const [search, setSearch] = React.useState('');
  const [sourceFilter, setSourceFilter] = React.useState<Cosmetic['type'] | 'all'>('all');
  const [selected, setSelected] = React.useState<Cosmetic | null>(null);
  const [activeIds, setActiveIds] = React.useState<Set<string>>(new Set());
  const [importing, setImporting] = React.useState(false);
  const packPromptedRef = React.useRef(false);

  useDetectOnMount();

  React.useEffect(() => {
    window.lex.ipc.cosmeticsGet().then((items) => {
      const g = makeGallery(Array.isArray(items) ? items : []);
      setGallery(g);
      setSelected((s) => (s ? g.items.find((c) => c.id === s.id) ?? s : g.items[0] ?? null));
    }).catch(() => {});
    const loadModules = () => {
      window.lex.ipc.modulesGet().then((mods) => {
        setActiveIds(new Set(Object.entries(mods).filter(([, v]) => v.enabled).map(([id]) => id)));
      }).catch(() => {});
    };
    loadModules();
    const off = window.lex?.on?.('lex:modules-updated', loadModules);
    return () => off?.();
  }, []);

  const filtered = React.useMemo(
    () =>
      gallery.items.filter(
        (c) =>
          (sourceFilter === 'all' || c.type === sourceFilter) &&
          (!search || c.name.toLowerCase().includes(search.toLowerCase()) || (c.description ?? '').toLowerCase().includes(search.toLowerCase())),
      ),
    [gallery, search, sourceFilter],
  );

  async function toggleCosmetic(c: Cosmetic) {
    const moduleId = `cosmetic-${c.id}`;
    const enabled = !activeIds.has(moduleId);
    await window.lex.ipc.modulesSetState({ id: moduleId, enabled });
    const updates = gallery.items.map((item) =>
      item.id === c.id ? { ...item, active: enabled } : item,
    );
    await window.lex.ipc.cosmeticsSave(updates).catch(() => {});
    setGallery(makeGallery(updates));
    setSelected((s) => (s ? updates.find((x) => x.id === s.id) ?? s : s));
    setActiveIds((prev) => {
      const next = new Set(prev);
      if (enabled) next.add(moduleId);
      else next.delete(moduleId);
      return next;
    });
    if (enabled && !packPromptedRef.current) {
      packPromptedRef.current = true;
      const opened = await window.lex?.ipc?.cosmeticsOpenPack?.();
      setStatus({
        text: opened?.ok
          ? `${c.name} equipado. Abre el pack en Minecraft y pulsa Activar para aplicarlo al personaje.`
          : `${c.name} equipado en el cliente. Activa el pack 3D desde Minecraft.`,
        tone: 'ok',
      });
      return;
    }
    setStatus({ text: enabled ? `${c.name} activo (se pueden combinar varios).` : `${c.name} desactivado.`, tone: 'ok' });
  }

  async function handleImport(files: File[]) {
    setImporting(true);
    const { importCosmeticsFromString } = await import('@lex/cosmetics');
    try {
      const results: ImportResult[] = await Promise.all(
        files.map(async (f) => {
          const t = await f.text();
          return importCosmeticsFromString(t);
        }),
      );
      const accepted = results.flatMap((r) => r.accepted);
      const rejected = results.flatMap((r) => r.rejected);
      if (accepted.length) {
        const items = [...gallery.items, ...accepted];
        await window.lex.ipc.cosmeticsSave(items).catch(() => {});
        setGallery(makeGallery(items));
        setStatus({
          text: `Importados: ${accepted.length}${rejected.length ? `, rechazados: ${rejected.length}` : ''}. Cosméticos privados: solo visibles en LEX CLIENT.`,
          tone: accepted.length ? 'ok' : 'warning',
        });
      } else {
        setStatus({ text: `Importados: 0, rechazados: ${rejected.length}`, tone: 'warning' });
      }
    } catch (e) {
      setStatus({ text: 'Error al importar cosméticos', tone: 'error' });
      console.error('[LEX] import error:', e);
    } finally {
      setImporting(false);
    }
  }

  const isActive = selected ? activeIds.has(`cosmetic-${selected.id}`) : false;
  const activeCount = gallery.items.filter((c) => activeIds.has(`cosmetic-${c.id}`)).length;

  return (
    <div className="mods-shell cosmetic-shell animate-slideUp">
      <header className="mods-head animate-slideUp">
        <div>
          <div className="mods-kicker">Cosmetics · LEX Client</div>
          <h1 className="mods-title">Cosméticos — LEX CLIENT</h1>
          <p className="mods-subtitle">
            Galería visual con atlas LEXTEX1 animados, renderizados por el overlay. Nada de cuentas, auth ni archivos del juego.
          </p>
        </div>
        <div className="mods-active-pill" title="Cosméticos activos en el overlay">
          <span className="status-dot ok" />
          <div><strong>{activeCount}</strong><span> / {gallery.items.length} activos</span></div>
          <div className="mods-active-track"><i style={{ width: `${gallery.items.length ? (activeCount / gallery.items.length) * 100 : 0}%` }} /></div>
        </div>
        <Button
          variant="secondary"
          iconLeft={<UiIcon name="folder" size={14} />}
          onClick={async () => {
            const r = await window.lex?.ipc?.cosmeticsOpenPack?.();
            setStatus({ text: r?.ok ? 'Abre el .mcpack con Minecraft para aplicar la geometría al jugador.' : 'No se pudo abrir el pack.', tone: r?.ok ? 'ok' : 'warning' });
          }}
        >
          GEOMETRÍA 3D · ABRIR MCPACK
        </Button>
      </header>

      <section className="cosmetic-showcase">
        <div className="cosmetic-showcase-preview">
          <AnimatedPreview cosmetic={selected} />
          <div className="cosmetic-stage-footer">
            <span>VISTA PREVIA EN VIVO</span>
            <span>{selected?.animated ? `${selected.frames ?? 1} FRAMES` : 'ESTÁTICO'}</span>
          </div>
        </div>
        <div className="cosmetic-detail-panel">
          <div className="cosmetic-detail-kicker">SELECCIÓN ACTUAL</div>
          <div className="cosmetic-detail-title">{selected ? selected.name : 'Sin cosmético seleccionado'}</div>
          <div className="cosmetic-detail-tags">
            <Pill tone="pink">{selected?.type ?? '—'}</Pill>
            <Pill tone={selected?.source === 'private' ? 'green' : 'amber'}>{selected?.source ?? '—'}</Pill>
            {selected && <Pill>v{selected.version ?? '0'}</Pill>}
            <Pill tone={isActive ? 'green' : 'neutral'}>{isActive ? 'ACTIVO' : 'INACTIVO'}</Pill>
          </div>
          <p className="cosmetic-detail-desc">
            {selected?.description ?? 'Elige una tarjeta de la galería para previsualizar su atlas animado.'}
          </p>
          <div className="cosmetic-meta-grid">
            <div><span>Animación</span><b>{selected?.animated ? `${selected.frames ?? 1}f · ${selected.frameMs ?? 0}ms` : 'estática'}</b></div>
            <div><span>Capas</span><b>{selected?.layers ?? 0}</b></div>
            <div><span>Atlas</span><b className="truncate-2">{selected?.atlas ?? 'importado'}</b></div>
            <div><span>Visibilidad</span><b>{selected?.source ?? '—'}</b></div>
          </div>
          <div className="cosmetic-detail-actions">
            <Button
              data-cosmetic-activate={selected?.id}
              disabled={!selected || !selected.atlas}
              onClick={() => selected && void toggleCosmetic(selected)}
            >
              {isActive ? `Desactivar ${selected?.name ?? ''}` : `Activar ${selected?.name ?? ''}`}
            </Button>
            {!selected?.atlas && (
              <span className="cosmetic-note">Este cosmético no trae atlas LEXTEX1: solo se puede previsualizar.</span>
            )}
          </div>
        </div>
      </section>

      <div className="mods-toolbar cosmetic-toolbar">
        <div className="mods-search">
          <UiIcon name="search" size={16} />
          <input
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Buscar cosméticos…"
            aria-label="Buscar cosméticos"
          />
        </div>
        <div className="cosmetic-filters">
          {(['all', 'local', 'resource-pack', 'import'] as const).map((s) => (
            <button
              key={s}
              type="button"
              className={`mod-chip ${sourceFilter === s ? 'active' : ''}`}
              aria-pressed={sourceFilter === s}
              onClick={() => setSourceFilter(s)}
            >
              {s === 'all' ? 'Todos' : s === 'local' ? 'Propios' : s === 'resource-pack' ? 'R. Pack' : 'Importados'}
            </button>
          ))}
        </div>
        <Button iconLeft={<UiIcon name="layers" size={14} />} onClick={() => document.getElementById('cosmetic-file-input')?.click()} disabled={importing}>
          {importing ? 'Importando…' : 'Importar cosméticos'}
        </Button>
        <input
          id="cosmetic-file-input"
          type="file"
          multiple
          className="hidden"
          accept="*.json,*/*"
          onChange={(e) => {
            const files = Array.from(e.target.files ?? []);
            if (files.length) void handleImport(files);
            e.target.value = '';
          }}
        />
      </div>

      <div className="cosmetic-grid" aria-label="Galería de cosméticos">
        {filtered.length === 0 ? (
          <div className="mods-empty cosmetic-empty">
            <h3>{search ? 'Sin cosméticos coincidentes.' : 'Biblioteca vacía.'}</h3>
            <p>Importa tu schema JSON propio: LEX CLIENT lo valida antes de mostrarlo.</p>
          </div>
        ) : (
          filtered.map((c) => {
            const active = activeIds.has(`cosmetic-${c.id}`);
            const isSelected = selected?.id === c.id;
            return (
              <button
                key={c.id}
                type="button"
                className={`cosmetic-card ${isSelected ? 'is-selected' : ''} ${active ? 'is-active' : ''}`}
                data-cat="cosmetics"
                onClick={() => setSelected(c)}
              >
                <AnimatedPreview cosmetic={c} compact />
                <div className="cosmetic-card-body">
                  <div className="cosmetic-card-title">{c.name}</div>
                  <div className="cosmetic-card-meta">
                    <Pill tone={active ? 'green' : 'neutral'}>{active ? 'ACTIVO' : c.type}</Pill>
                    {c.animated && <Pill tone="pink">{c.frames ?? 1}f</Pill>}
                  </div>
                  <div className="cosmetic-card-desc">{c.description ?? 'Cosmético animado de LEX CLIENT.'}</div>
                </div>
              </button>
            );
          })
        )}
      </div>

      <footer className="mods-legend cosmetic-legend">
        <span className="dot" style={{ background: '#ff8bd0' }} /> Atlas propios LEXTEX1 (BGRA premultiplicado)
        <span className="dot" style={{ background: '#5bd68e', marginLeft: 10 }} /> Activo en el juego
        <span style={{ marginLeft: 'auto' }}>Privado por diseño: sin auth, sin archivos del juego</span>
      </footer>
    </div>
  );
}
