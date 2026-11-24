import React from 'react';
import { Card, Toggle, Button } from '@lex/ui-components';
import { UiIcon, type IconName } from '../components/InterfaceIcons';
import { runDiagnostics, applyFix } from '../utils/diagnostics';
import { checkUpdates } from '../utils/updater';
import { setOverlayState } from '../utils/overlayBridge';
import { useAppHelpers } from '../stores/appStore';
import { useDetectOnMount } from '../hooks/useDetectOnMount';

const TABS: Array<{ id: string; label: string; icon: IconName }> = [
  { id: 'general', label: 'General', icon: 'home' },
  { id: 'aparencia', label: 'Apariencia', icon: 'visual' },
  { id: 'overlays', label: 'Overlays', icon: 'layers' },
  { id: 'perf', label: 'Rendimiento', icon: 'pulse' },
  { id: 'controles', label: 'Controles', icon: 'keyboard' },
  { id: 'actualizaciones', label: 'Actualizaciones', icon: 'box' },
  { id: 'directorios', label: 'Directorios', icon: 'folder' },
  { id: 'perfiles', label: 'Perfiles', icon: 'shield' },
  { id: 'diagnostico', label: 'Diagnóstico', icon: 'pulse' },
];

export function Settings() {
  const [tab, setTab] = React.useState('general');
  const [diagReport, setDiagReport] = React.useState<import('@lex/shared').DiagnosticReport | null>(null);
  const { setStatus, setTheme } = useAppHelpers();
  useDetectOnMount();

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
    <div className="set-shell animate-slideUp">
      <header className="set-head">
        <div>
          <div className="home-eyebrow"><UiIcon name="gear" size={13} /> SISTEMA</div>
          <h1>Ajustes</h1>
          <p>Configuración del launcher, overlay y controles del ClickGUI.</p>
        </div>
        <span className="set-head-mark"><UiIcon name="gear" size={28} /></span>
      </header>

      <nav className="set-tabs" aria-label="Secciones de ajustes">
        {TABS.map((t) => (
          <button key={t.id} type="button" className={`set-tab ${tab === t.id ? 'is-active' : ''}`} aria-pressed={tab === t.id} onClick={() => setTab(t.id)}>
            <UiIcon name={t.icon} size={14} /> {t.label}
          </button>
        ))}
      </nav>

      <main className="set-content">
        {tab === 'general' && (
          <>
            <Card title="Bienvenida" subtitle="Tema del launcher">
              <div className="set-row">
                <div className="set-row-main"><b>Tema oscuro</b><small>Identidad visual única en launcher y overlay.</small></div>
                <Toggle label="" on={true} onChange={() => setTheme('dark')} />
              </div>
              <p className="set-note">Idiomas: es, en — se añaden en Fase 7.</p>
            </Card>
            <Card title="Instalación local" subtitle="Directorio de datos del launcher (%APPDATA%\lex-client)">
              <code className="set-code">%APPDATA%\lex-client\ — configs reales del cliente</code>
            </Card>
          </>
        )}

        {tab === 'aparencia' && (
          <>
            <Card title="Diseño de LEX CLIENT" subtitle="Azul marino profundo y acento #315783 sobre fondo #080B12.">
              <div className="set-palette">
                <div><span>Base</span><i style={{ background: '#080B12' }} /><code>#080B12</code></div>
                <div><span>Tarjeta</span><i style={{ background: '#101827' }} /><code>#101827</code></div>
                <div><span>Superficie</span><i style={{ background: '#172338' }} /><code>#172338</code></div>
                <div><span>Acento</span><i style={{ background: '#315783' }} /><code>#315783</code></div>
              </div>
            </Card>
            <Card title="Logotipo oficial" subtitle="Importa el PNG original: se usa en el menú principal y en el ClickGUI sin redibujarlo.">
              <div className="set-actions">
                <Button variant="primary" iconLeft={<UiIcon name="shield" size={14} />} onClick={async () => {
                  const r = await window.lex?.ipc?.logoPick?.();
                  setStatus({ text: r?.ok ? 'Logotipo importado.' : 'Importación cancelada.', tone: r?.ok ? 'ok' : 'warning' });
                }}>
                  Importar logotipo PNG
                </Button>
              </div>
              <p className="set-note">El PNG se guarda en %APPDATA%\lex-client y se prepara para el overlay. Fondo negro sólido.</p>
            </Card>
            <Card title="Tipografía" subtitle="Inter para interfaz · JetBrains Mono para datos y display">
              <div className="set-type"><b>DISPLAY / 700</b><span>Inter / JetBrains Mono</span></div>
              <p className="set-note">La misma jerarquía se replica en el overlay GDI: título, dato, etiqueta.</p>
            </Card>
          </>
        )}

        {tab === 'overlays' && (
          <>
            <Card title="Overlay externo" subtitle="Ventana GDI nativa sobre Minecraft">
              <div className="set-row">
                <div className="set-row-main"><b>Mostrar overlay</b><small>HUD, CrossGUI y cosméticos client-side.</small></div>
                <Toggle label="" on={true} onChange={handleOverlayToggle} />
              </div>
              <div className="set-actions">
                <Button variant="secondary" iconLeft={<UiIcon name="shield" size={14} />} onClick={async () => { await setOverlayState({ clickThrough: true }); setStatus({ text: 'Click-through activado.', tone: 'ok' }); }}>
                  Activar click-through
                </Button>
              </div>
              <p className="set-note">Motor: C++ Win32 + GDI sobre ventana layered. Sin inyección en el juego para el HUD.</p>
            </Card>
            <Card title="Overlays internos — Módulos HUD">
              <p className="set-note"><UiIcon name="arrow" size={13} /> Catálogo disponible en la pantalla «Módulos».</p>
              <p className="set-note">Estado persistido en %APPDATA%\lex-client\modules.json</p>
            </Card>
          </>
        )}

        {tab === 'perf' && (
          <Card title="Bucle de monitoreo" subtitle="Frecuencia de sondeo del overlay">
            <div className="set-actions">
              {[200, 500, 1000].map((ms) => (
                <Button key={ms} variant="secondary" size="sm" onClick={() => setStatus({ text: `Monitoreo a ${ms}ms activado.`, tone: 'ok' })}>{ms}ms</Button>
              ))}
            </div>
            <p className="set-note">FPS reales del juego vía puente DX11/DX12; CPS con ventana deslizante de 1000 ms.</p>
          </Card>
        )}

        {tab === 'controles' && (
          <Card title="Controles del ClickGUI" subtitle="Una sola tecla, todo lo demás con clic">
            <div className="set-control-grid">
              <div><span className="set-key">F8</span><b>Abrir / cerrar</b><small>Única tecla del cliente.</small></div>
              <div><span className="set-key"><UiIcon name="move" size={15} /></span><b>Mover HUD</b><small>Arrastra paneles y guarda su posición.</small></div>
              <div><span className="set-key"><UiIcon name="keyboard" size={15} /></span><b>Buscar</b><small>Teclado en pantalla; el juego nunca recibe teclas del menú.</small></div>
              <div><span className="set-key"><UiIcon name="grid" size={15} /></span><b>Cerrar</b><small>Botón × en la cabecera o en el riel.</small></div>
            </div>
            <p className="set-note">F8 abre y cierra el menú. Dentro del menú no hay atajos: activar módulos, buscar, mover paneles del HUD y cambiar de pestaña se hace con clic.</p>
          </Card>
        )}

        {tab === 'actualizaciones' && (
          <Card title="Launcher" subtitle="Actualizador independiente del juego (CHANGELOG local)">
            <div className="set-actions"><Button variant="secondary" onClick={handleUpdates}>Buscar actualización</Button></div>
            <p className="set-note">Fase 2 implementado: compara la versión del launcher con CHANGELOG.md. Actualización automática: Fase 7.</p>
          </Card>
        )}

        {tab === 'directorios' && (
          <Card title="Directorio de datos (userData)">
            <code className="set-code">%LOCALAPPDATA%\Packages\LEXClient_7z28wekyb3d8bbwe</code>
            <p className="set-note">Configs: %APPDATA%\lex-client\config.json — también visible en diagnóstico.</p>
          </Card>
        )}

        {tab === 'perfiles' && (
          <Card title="Perfiles" subtitle="Snapshots de estado del launcher (pendiente de Fase 6)">
            <div className="set-row"><div className="set-row-main"><b>Perfil «Por defecto»</b><small>Snapshot por instalación de Minecraft (pendiente).</small></div><span className="lex-badge lex-badge-default">Pendiente</span></div>
          </Card>
        )}

        {tab === 'diagnostico' && (
          <>
            <Card title="Diagnóstico" subtitle="Datos públicos: OS, arch, Node, versión del launcher y versiones detectadas. Sin info de cuenta.">
              <div className="set-actions"><Button iconLeft={<UiIcon name="pulse" size={14} />} onClick={handleDiagnostics}>Ejecutar diagnóstico</Button></div>
            </Card>
            {diagReport?.diagnostics.map((d) => (
              <div key={d.id} className="set-diag">
                <span className={`set-diag-dot ${d.level === 'ok' ? 'ok' : 'warn'}`} />
                <div className="set-diag-main"><b>{d.message}</b>{d.suggest && <small>{d.suggest}</small>}</div>
                <Button variant="secondary" size="sm" onClick={() => handleApplyFix(d.id)}>Reparar</Button>
              </div>
            ))}
          </>
        )}
      </main>
    </div>
  );
}
