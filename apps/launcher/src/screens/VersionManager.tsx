import { Card, Badge } from '@lex/ui-components';
import { UiIcon } from '../components/InterfaceIcons';
import type { MinecraftEdition } from '@lex/shared';
import { useApp, useAppHelpers } from '../stores/appStore';
import { useDetectOnMount } from '../hooks/useDetectOnMount';

function editionBadge(edition: MinecraftEdition, isPreview: boolean) {
  return <Badge variant={isPreview ? 'warning' : edition === 'education' ? 'default' : 'success'}>{isPreview ? 'Preview' : edition}</Badge>;
}

export function VersionManager() {
  const { installations, latest, loading } = useApp();
  const { start, finish } = useAppHelpers();
  const isLoading = loading.detect;
  useDetectOnMount();

  async function refresh() {
    start('detect');
    try {
      await window.lex.ipc.detect();
    } finally {
      finish('detect');
    }
  }

  const sorted = [...installations].sort((a, b) => b.version.localeCompare(a.version));

  return (
    <div className="ver-shell animate-slideUp">
      <header className="ver-head">
        <div>
          <div className="home-eyebrow"><UiIcon name="box" size={13} /> DETECCIÓN LOCAL</div>
          <h1>Versiones — Minecraft</h1>
          <p>Instalaciones reales detectadas desde Get-AppxPackage y escaneo de archivos.</p>
        </div>
        <button type="button" className="lex-btn lex-btn-secondary lex-btn-md" onClick={refresh} disabled={isLoading}>
          <UiIcon name="pulse" size={14} /> {isLoading ? 'Buscando…' : 'Refrescar detección'}
        </button>
      </header>

      <Card title="Instalación recomendada" subtitle="Última versión estable detectada en el equipo">
        <div className="ver-feature">
          <span className="ver-feature-icon"><UiIcon name="target" size={22} /></span>
          <div><b>{latest?.displayName ?? 'Ninguna'}</b><small>{latest?.version ? `Versión ${latest.version}` : 'Sin detectar'}</small></div>
          {latest && editionBadge(latest.edition, latest.isPreview)}
        </div>
      </Card>

      {sorted.length === 0 ? (
        <Card className="ver-empty">
          <UiIcon name="box" size={26} />
          <p>No se encontró ninguna instalación de Minecraft for Windows.</p>
        </Card>
      ) : (
        <Card title={`Instalaciones detectadas — ${sorted.length}`} className="ver-table-card">
          <div className="ver-table">
            <div className="ver-table-head"><span>Paquete / Versión</span><span>Edición</span><span>Ruta de ejecución</span></div>
            {sorted.map((inst) => (
              <div key={inst.id} className="ver-table-row">
                <div>
                  <b>{inst.displayName}</b>
                  <code>{inst.packageId}</code>
                  <small>{inst.version}</small>
                </div>
                <div>{editionBadge(inst.edition, inst.isPreview)}</div>
                <code className="ver-path">{inst.executable ?? 'Sin ruta resuelta — pendiente de recomposición'}</code>
              </div>
            ))}
          </div>
        </Card>
      )}
    </div>
  );
}
