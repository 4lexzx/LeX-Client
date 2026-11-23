import React from 'react';
import { Card, Badge, Button } from '@lex/ui-components';
import { useDetectOnMount } from '../hooks/useDetectOnMount';
import { sampleGallery, type Gallery, type Cosmetic, type ImportResult } from '@lex/cosmetics';
import { useAppHelpers } from '../stores/appStore';

function Viewer3D({ cosmetic }: { cosmetic: Cosmetic | null }) {
  const canvasRef = React.useRef<HTMLCanvasElement>(null);
  React.useEffect(() => {
    if (!canvasRef.current) return;
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    let angle = 0;
    const draw = () => {
      const w = canvas.width = canvas.clientWidth || 400;
      const h = canvas.height = canvas.clientHeight || 240;
      ctx.clearRect(0, 0, w, h);
      angle += 0.03;
      const cx = w / 2;
      const cy = h / 2;
      const r = Math.min(w, h) * 0.3;
      ctx.save();
      ctx.translate(cx, cy);
      ctx.rotate(Math.sin(angle));
      // Capa (body): top
      ctx.fillStyle = '#00C8E8';
      ctx.strokeStyle = '#FFFFFF55';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.ellipse(0, -h / 8, r * 0.6, r * 0.45, 0.35 * Math.sin(angle), 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
      // Capa (accessory): bottom
      ctx.fillStyle = '#7C6FFF';
      ctx.beginPath();
      ctx.ellipse(0, h / 8, r * 0.5, r * 0.3, 0.2 * Math.sin(angle), 0, Math.PI * 2);
      ctx.fill();
      // Partícula flotante
      ctx.beginPath();
      ctx.arc(-r * 0.45, -r * 0.25, 6, 0, Math.PI * 2);
      ctx.fillStyle = '#22D3EE';
      ctx.fill();
      ctx.restore();
      if (cosmetic) {
        ctx.font = '12px Inter, sans-serif';
        ctx.fillStyle = '#A5A5C5';
        ctx.textAlign = 'center';
        ctx.fillText(cosmetic.name, 0, cy + h / 3);
        ctx.fillText(`v${cosmetic.version ?? '0'} — ${cosmetic.type}`, 0, cy + h / 2);
      }
      if (!cosmetic || cosmetic.layers === 0) {
        ctx.fillStyle = '#4A4F5E';
        ctx.font = '14px Inter, sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('Sin cosmético seleccionado', 0, cy + h / 3);
      }
    };
    const t = setInterval(draw, 32);
    draw();
    return () => clearInterval(t);
  }, [cosmetic]);

  return <canvas ref={canvasRef} width={400} height={240} className="rounded-2xl border border-[var(--border)] bg-[var(--bg-secondary)] w-full max-w-xs aspect-[4/3] object-contain" />;
}

export function Cosmetics() {
  const { setStatus } = useAppHelpers();
  const [gallery, setGallery] = React.useState<Gallery>(sampleGallery);
  const [search, setSearch] = React.useState('');
  const [sourceFilter, setSourceFilter] = React.useState<Cosmetic['type'] | 'all'>('all');
  const [selected, setSelected] = React.useState<Cosmetic | null>(null);
  const [importing, setImporting] = React.useState(false);

  useDetectOnMount();

  React.useEffect(() => {
    window.lex.ipc.cosmeticsGet().then((items) => {
      if (Array.isArray(items) && items.length) {
        const list = items as Cosmetic[];
        const privateCount = list.filter((c) => c.source === 'private').length;
        setGallery({ items: list, total: list.length, privateCount, sharedCount: list.length - privateCount });
      }
    }).catch(() => {});
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
        window.lex.ipc.cosmeticsSave(items).catch(() => {});
        setGallery({
          items,
          total: items.length,
          privateCount: items.filter((c) => c.source === 'private').length,
          sharedCount: items.filter((c) => c.source === 'shared').length,
        });
      }
      setStatus({
        text: `Importados: ${accepted.length}${rejected.length ? `, rechazados: ${rejected.length}` : ''}${accepted.length ? '. Todos privados: solo visible en clientes de LEX CLIENT.' : ''}`,
        tone: accepted.length ? 'ok' : 'warning',
      });
    } catch (e) {
      setStatus({ text: 'Error al importar cosméticos', tone: 'error' });
      console.error('[LEX] import error:', e);
    } finally {
      setImporting(false);
    }
  }

  return (
    <div className="flex flex-col gap-6 p-8 animate-slideUp">
      {/* Encabezado */}
      <div className="flex flex-col gap-2">
        <h1 className="text-3xl font-bold text-[var(--text)]">Cosméticos — LEX CLIENT</h1>
        <p className="text-sm text-[var(--text-secondary)]">
          Sistema propio. Cosméticos privados: visibles solo en clientes de LEX CLIENT. No alteramos cuentas, auth ni archivos del juego.
        </p>
      </div>

      {/* Controles: buscar + filtro */}
      <div className="flex flex-col gap-3 sm:flex-row">
        <input
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          placeholder="Buscar cosméticos…"
          className="flex-1 rounded-lg bg-[var(--surface)] border border-[var(--border)] px-4 py-2.5 text-sm text-[var(--text)] placeholder:text-[var(--text-muted)] outline-none focus:border-[var(--accent)] transition"
        />
        <div className="flex gap-2">
          {(['all', 'local', 'resource-pack', 'import'] as const).map((s) => (
            <button
              key={s}
              className={`px-3 py-1.5 rounded-lg text-xs font-medium transition ${sourceFilter === s ? 'bg-[var(--accent)] text-white' : 'bg-[var(--surface-2)] text-[var(--text-secondary)] hover:text-[var(--text)]'}`}
              onClick={() => setSourceFilter(s)}
            >
              {s === 'all' ? 'Todos' : s === 'local' ? 'Propios' : s === 'resource-pack' ? 'R. Pack' : 'Importados'}
            </button>
          ))}
        </div>
        <Button onClick={() => document.getElementById('cosmetic-file-input')?.click()} disabled={importing}>
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

      {/* Viewer 3D */}
      <Card title="Previsualización del cosmético seleccionado">
        <div className="flex flex-col sm:flex-row gap-6 items-start">
          <Viewer3D cosmetic={selected} />
          <div className="flex-1">
            <div className="text-sm font-semibold mb-1">
              {selected ? `${selected.name} (${selected.type})` : 'Sin selección'}
            </div>
            {selected && <div className="text-xs text-[var(--text-muted)]">Versión: {selected.version ?? '0'}</div>}
            {selected && selected.description && <div className="text-sm text-[var(--text-secondary)] mt-2">{selected.description}</div>}
            <div className="mt-3 text-xs text-[var(--text-muted)] space-y-1">
              <div>Visibilidad: <Badge variant={selected?.source === 'shared' ? 'warning' : 'success'}>{selected?.source ?? 'ninguno'}</Badge></div>
              <div>Layers: {selected?.layers ?? 0} (0 = cosmético local puro)</div>
            </div>
          </div>
        </div>
      </Card>

      {/* Biblioteca */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
        {filtered.length === 0 ? (
          <Card className="col-span-1 md:col-span-3 text-center py-12">
            <p className="text-sm text-[var(--text-secondary)]">{search ? 'Sin cosméticos coincidentes.' : 'Biblioteca de cosméticos vacía. Importa tu schema JSON propio (validado por LEX CLIENT).'}</p>
          </Card>
        ) : (
          filtered.map((c: Cosmetic) => (
            <Card
              key={c.id}
              title={c.name}
              subtitle={`Tipo: ${c.type}`}
              action={
                <button
                  className={`px-2.5 py-1 rounded-full text-[10px] font-semibold uppercase transition ${selected?.id === c.id ? 'bg-[var(--accent)] text-white' : 'bg-[var(--surface-2)] text-[var(--text-muted)]'}`}
                  onClick={() => setSelected((s: Cosmetic | null) => (s?.id === c.id ? null : c))}
                >
                  {selected?.id === c.id ? 'Seleccionado' : 'Vista previa'}
                </button>
              }
            >
              <div className="text-xs text-[var(--text-muted)]">Estado: {c.type === 'import' ? 'importado' : 'propio'} — en la Galería local de LEX CLIENT.</div>
              <div className="text-xs text-[var(--text-muted)] mt-1">
                <Badge variant="accent">{c.type}</Badge> {c.version ? `v${c.version}` : ''}
              </div>
            </Card>
          ))
        )}
      </div>

      {/* Banner de regla de privacidad */}
      <div className="rounded-lg border border-[var(--border)] bg-[var(--surface-2)] px-4 py-3 text-[10px] text-[var(--text-muted)]">
        Regla de privacidad: cosméticos propios se aplican solo en clientes de LEX CLIENT (individuales). Para aplicar cosmética de otros, se requiere redistribución de resource packs e instalación en el cliente — LEX CLIENT no modifica archivos del juego ni cuentas de Mojang/Microsoft.
      </div>
    </div>
  );
}

