import React from 'react';
import { Card, Toggle, Button } from '@lex/ui-components';
import { runDiagnostics, applyFix } from '../utils/diagnostics';
import { checkUpdates } from '../utils/updater';
import { setOverlayState } from '../utils/overlayBridge';
import { useAppHelpers } from '../stores/appStore';
import { useDetectOnMount } from '../hooks/useDetectOnMount';

function Tab({ label, active, onClick }: { label: string; active: boolean; onClick: () => void }) {
  return (
    <button
      className={`text-sm px-4 py-3 border-b-2 transition-colors ${active ? 'border-b-[var(--accent)] text-white font-semibold' : 'text-[var(--text-secondary)] border-b-transparent hover:text-[var(--text)]'}`}
      onClick={onClick}
    >
      {label}
    </button>
  );
}

export function Settings() {
  const [tab, setTab] = React.useState('general');
  const [diagReport, setDiagReport] = React.useState<import('@lex/shared').DiagnosticReport | null>(null);
  const { setStatus, setTheme } = useAppHelpers();
  useDetectOnMount();

  const tabs = [
    { id: 'general', label: 'General' },
    { id: 'aparencia', label: 'Apariencia' },
    { id: 'overlays', label: 'Overlays' },
    { id: 'perf', label: 'Rendimiento' },
    { id: 'controles', label: 'Controles' },
    { id: 'actualizaciones', label: 'Actualizaciones' },
    { id: 'directorios', label: 'Directorios' },
    { id: 'perfiles', label: 'Perfiles' },
    { id: 'diagnostico', label: 'Diagnóstico' },
  ] as const;

  async function handleDiagnostics() {
    setStatus({ text: 'Ejecutando diagnóstico…', tone: 'muted' });
    try {
      const r = await runDiagnostics();
      setDiagReport(r);
      const bad = r.diagnostics.filter((d) => d.level !== 'ok');
      setStatus({ text: bad.length ? `${bad.length} problema(s) encontrado` : 'Sin problemas detectados', tone: bad.length ? 'warning' : 'ok' });
      return r;
    } catch (e) {
      console.error('[LEX] diagnostics error:', e);
      setStatus({ text: 'Error al ejecutar diagnóstico.', tone: 'error' });
      return null;
    }
  }

  async function handleApplyFix(id: string) {
    const r = await applyFix(id);
    if (r.applied) setStatus({ text: 'Corrección aplicada.', tone: 'ok' });
    else setStatus({ text: r.note ?? 'Sin corrección aplicable.', tone: 'warning' });
  }

  async function handleUpdates() {
    const u = await checkUpdates();
    if (u.changed) setStatus({ text: `Actualización disponible: ${u.latest}`, tone: 'warning' });
    else setStatus({ text: `Launcher ${u.launcher}. stable ${u.latest}.`, tone: 'ok' });
  }

  async function handleOverlayToggle(v: boolean) {
    await setOverlayState({ show: v });
    setStatus({ text: v ? 'Overlay externo encendido.' : 'Overlay externo apagado.', tone: 'ok' });
  }

  return (
    <div className="flex min-h-full flex-col animate-slideUp">
      <div className="h-0.5 w-full" style={{ background: 'linear-gradient(90deg, var(--accent), var(--accent) 30%, transparent 30%, transparent 100%)' }} />
      <header className="flex flex-wrap items-center gap-2 border-b border-[var(--border)] pb-4">
        <div className="text-xl font-bold">Ajustes</div>
        {tabs.map((t) => (
          <Tab key={t.id} label={t.label} active={tab === t.id} onClick={() => setTab(t.id)} />
        ))}
        <div className="flex-1" />
      </header>
      <main className="p-8 space-y-6">
        {tab === 'general' && (
          <div className="flex flex-col gap-6">
            <Card title="Bienvenida" subtitle="Tema del launcher">
              <Toggle label="Tema oscuro" on={true} onChange={() => setTheme('dark')} />
              <p className="text-xs text-[var(--text-muted)] mt-3">Idiomas: es, en — se añaden en Fase 7.</p>
            </Card>
            <Card title="Instalación local" subtitle="Directorio de datos del launcher (%APPDATA%\lex-client)">
              <div className="font-mono text-xs text-[var(--text-secondary)] p-3 rounded bg-[var(--bg-secondary)] break-all">
                %APPDATA%\lex-client\ — pending: see diagnostics tab (userData path exposed via diagnostics).
              </div>
            </Card>
          </div>
        )}

        {tab === 'aparencia' && (
          <div className="flex flex-col gap-6">
            <Card title="Diseño de LEX CLIENT" subtitle="Paleta propia: azul eléctrico #00C8E8 sobre fondo #101116. Sin copiar interfaces de clientes de Java.">
              <div className="flex gap-2 flex-wrap">
                <div className="flex flex-col gap-1">
                  <span className="text-[10px] uppercase tracking-wide text-[var(--text-muted)]">Primaria</span>
                  <span className="w-14 h-6 rounded-lg" style={{ background: '#00C8E8' }} />
                  <span className="w-14 h-6 rounded-lg" style={{ background: '#101116' }} />
                </div>
                <div className="flex flex-col gap-1">
                  <span className="text-[10px] uppercase tracking-wide text-[var(--text-muted)]">Secundaria</span>
                  <span className="w-14 h-6 rounded-lg" style={{ background: '#181A22' }} />
                  <span className="w-14 h-6 rounded-lg" style={{ background: '#20232D' }} />
                </div>
              </div>
            </Card>
            <Card title="Tipografía" subtitle="Inter (renderer) + JetBrains Mono (display)">
              <p className="text-2xl font-bold text-[var(--text)]">Display — JetBrains Mono Bold</p>
              <p className="text-sm text-[var(--text-secondary)] mt-2">Body — Inter Regular</p>
            </Card>
          </div>
        )}

        {tab === 'overlays' && (
          <div className="flex flex-col gap-6">
            <Card title="Overlay externo (Fase 3)" subtitle="Ventana nativa transparente sobre Minecraft">
              <div className="flex gap-3">
                <Toggle label="Mostrar overlay" on={true} onChange={handleOverlayToggle} />
                <Button variant="secondary" onClick={async () => { await setOverlayState({ clickThrough: true }); setStatus({ text: 'Click-through activado.', tone: 'ok' }); }}>Activar click-through</Button>
              </div>
              <p className="text-xs text-[var(--text-muted)] mt-3">Motor: C++ Win32 + Direct2D — compilado en MSVC. Detección de Minecraft integrada.</p>
            </Card>
            <Card title="Overlays internos — Módulos HUD (Fase 4)">
              <p className="text-xs text-[var(--text-secondary)]">→ Catalog de módulos disponible en la pantalla de navegación: "Módulos".</p>
              <p className="text-xs text-[var(--text-muted)] mt-2">Estado de módulos: persistido en %APPDATA%\lex-client\modules.json</p>
            </Card>
          </div>
        )}

        {tab === 'perf' && (
          <div className="flex flex-col gap-6">
            <Card title="Bucle de monitoreo">
              <div className="flex flex-wrap gap-2">
                {[200, 500, 1000].map((ms) => (
                  <Button key={ms} variant="secondary" size="sm" onClick={() => setStatus({ text: `Monitoreo a ${ms}ms activado.`, tone: 'ok' })}>
                    {ms}ms
                  </Button>
                ))}
              </div>
            </Card>
          </div>
        )}

        {tab === 'controles' && (
          <div className="flex flex-col gap-6">
            <Card title="Atajos de teclado (Keybinds)">
              <div className="text-sm text-[var(--text-secondary)]">Bindings se registran por módulo activo. Desde Fase 4.</div>
            </Card>
          </div>
        )}

        {tab === 'actualizaciones' && (
          <div className="flex flex-col gap-6">
            <Card title="Launcher" subtitle="Actualizador independiente del juego (CHANGELOG local)">
              <Button variant="secondary" onClick={handleUpdates}>Buscar actualización</Button>
              <p className="text-xs text-[var(--text-muted)] mt-3">
                Fase 2 implementado: compara la versión del launcher con CHANGELOG.md. Actualización automática: Fase 7.
              </p>
            </Card>
          </div>
        )}

        {tab === 'directorios' && (
          <div className="flex flex-col gap-6">
            <Card title="Directorio de datos (userData)">
              <div className="font-mono text-xs text-[var(--text-secondary)] p-3 rounded bg-[var(--bg-secondary)] break-all">
                %LOCALAPPDATA%\Packages\LEXClient_7z28wekyb3d8bbwe — también visible en diagnóstico.
              </div>
              <p className="text-xs text-[var(--text-muted)] mt-2">Configs: %APPDATA%\lex-client\config.json</p>
            </Card>
          </div>
        )}

        {tab === 'perfiles' && (
          <div className="flex flex-col gap-6">
            <Card title="Perfiles" subtitle="Snapshots de estado del launcher (pendiente de Fase 6)">
              <div className="text-sm text-[var(--text-secondary)]">Lista de perfiles: pendiente de Fase 6 (persistencia JSON + UI).</div>
              <div className="text-sm text-[var(--text-muted)] mt-1">Perfil "Por defecto" — snapshot por instalación de Minecraft (pendiente)</div>
            </Card>
          </div>
        )}

        {tab === 'diagnostico' && (
          <div className="flex flex-col gap-6">
            <Button onClick={handleDiagnostics}>Ejecutar diagnóstico</Button>
            <div className="flex flex-col gap-2">
              <span className="text-xs text-[var(--text-muted)]">Datos públicos (sin info de cuenta): OS, arch, Node, versión del launcher, versiones de Minecraft detectadas.</span>
              <span className="text-xs text-[var(--text-muted)]">Fase 2: podemos reparar ítems listados abajo.</span>
            </div>
            {diagReport?.diagnostics.map((d) => (
              <div key={d.id} className="flex items-center justify-between gap-3 rounded-lg bg-[var(--surface-2)]/40 border border-[var(--border)] px-4 py-3">
                <div className="min-w-0">
                  <div className="text-sm text-[var(--text)] flex items-center gap-2">{d.message}</div>
                  {d.suggest && <div className="text-xs text-[var(--text-muted)] mt-0.5">{d.suggest}</div>}
                </div>
                <Button variant="secondary" size="sm" onClick={() => handleApplyFix(d.id)}>
                  Reparar
                </Button>
              </div>
            ))}
          </div>
        )}
      </main>
    </div>
  );
}
