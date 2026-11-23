import { Card, Badge } from '@lex/ui-components';
import type { MinecraftInstallation, MinecraftEdition } from '@lex/shared';
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
      const r = await window.lex.ipc.detect();
      // Detectamos sin setInstallations guardado en el state global? mismo reducer desde main — deseable: verificamos.
      void r;
    } finally {
      finish('detect');
    }
  }

  const sorted = [...installations].sort((a, b) => b.version.localeCompare(a.version));

  return (
    <div className="flex flex-col gap-6 p-8 animate-slideUp">
      <div className="flex items-center justify-between">
        <h1 className="text-3xl font-bold">Versiones — Minecraft</h1>
        <button
          className="px-3 py-1.5 rounded-lg bg-[var(--surface-2)] text-xs font-medium text-[var(--text-secondary)] hover:bg-[var(--surface)] transition"
          onClick={refresh}
          disabled={isLoading}
        >
          {isLoading ? 'Buscando…' : 'Refrescar detección'}
        </button>
      </div>

      <Card title="Detección" subtitle="Instalaciones detectadas desde Get-AppxPackage y escaneo de archivos (retail, preview, education)">
        <div className="text-sm text-[var(--text-secondary)]">Última estable más reciente detectada: <strong>{latest?.displayName ?? 'Ninguna'}</strong> {latest?.version ? `(${latest.version})` : ''} — {editionBadge(latest?.edition ?? 'retail', latest?.isPreview ?? false)}</div>
      </Card>

      {sorted.length === 0 ? (
        <Card className="text-center py-12">
          <p className="text-sm text-[var(--text-secondary)]">No se encontró ninguna instalación de Minecraft for Windows.</p>
        </Card>
      ) : (
        <Card title={`Instalaciones detectadas — ${sorted.length}`} className="overflow-hidden">
          <table className="w-full text-sm">
            <thead>
              <tr className="text-left text-[var(--text-muted)] text-xs uppercase tracking-wider border-b border-[var(--border)]">
                <th className="py-3 pr-4 font-semibold">Paquete / Versión</th>
                <th className="py-3 pr-4 font-semibold">Edición</th>
                <th className="py-3 pr-4 font-semibold">Ruta de ejecución</th>
              </tr>
            </thead>
            <tbody>
              {sorted.map((inst: MinecraftInstallation) => (
                <tr key={inst.id} className="border-b border-[var(--border)]/50 last:border-b-0 hover:bg-[var(--surface-2)]/30 transition">
                  <td className="py-3 pr-4">
                    <div className="font-medium">{inst.displayName}</div>
                    <div className="text-xs font-mono text-[var(--text-muted)] truncate max-w-xs">{inst.packageId}</div>
                    <div className="text-xs text-[var(--accent)] font-mono mt-0.5">{inst.version}</div>
                  </td>
                  <td className="py-3 pr-4">{editionBadge(inst.edition, inst.isPreview)}</td>
                  <td className="py-3 pr-4 text-[10px] font-mono text-[var(--text-muted)] truncate max-w-md">
                    {inst.executable ?? 'Sin ruta resuelta — recomponer de archivo (pending resolve)'}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </Card>
      )}
    </div>
  );
}
