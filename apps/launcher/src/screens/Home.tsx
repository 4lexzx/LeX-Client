import React from 'react';
import { Card } from '@lex/ui-components';
import { PlayButton } from '../components/PlayButton';
import { DetectionStatus } from '../components/DetectionStatus';
import { useApp, useAppHelpers } from '../stores/appStore';
import { useDetectOnMount } from '../hooks/useDetectOnMount';
import { checkUpdates } from '../utils/updater';
import { runDiagnostics } from '../utils/diagnostics';

export function Home() {
  const { installations, latest, loading, status } = useApp();
  const { setStatus } = useAppHelpers();
  const isLoading = loading.detect;

  useDetectOnMount();

  async function handleUpdatesStatus() {
    try {
      const u = await checkUpdates();
      if (u.changed) setStatus({ text: `Nueva versión del launcher disponible: ${u.latest}`, tone: 'warning' });
    } catch {
      /* no-op */
    }
  }
  React.useEffect(() => {
    void handleUpdatesStatus();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [isLoading, latest]);

  async function handleDiagnostics() {
    try {
      const r = await runDiagnostics();
      const bad = r.diagnostics.filter((d) => d.level !== 'ok');
      setStatus({ text: bad.length ? `${bad.length} problema(s) en diagnóstico` : 'Sin problemas detectados', tone: bad.length ? 'warning' : 'ok' });
    } catch {
      setStatus({ text: 'Error al ejecutar diagnóstico', tone: 'error' });
    }
  }

  return (
    <div className="flex flex-col gap-6 p-8 animate-slideUp">
      {/* Encabezado */}
      <div className="flex flex-col gap-2">
        <h1 className="text-3xl font-bold text-[var(--text)]">LEX CLIENT</h1>
        <p className="text-sm text-[var(--text-secondary)]">Cliente moderno y modular para Minecraft Bedrock Edition en Windows 10/11.</p>
      </div>

      {/* Estado de detección */}
      <Card
        title="Estado de la instalación"
        subtitle={latest ? `Última versión detectada: ${latest.displayName} ${latest.version}` : ''}
        action={<DetectionStatus installations={installations} isLoading={isLoading} />}
      >
        <div className="grid gap-3 sm:grid-cols-3">
          <div className="rounded-lg bg-[var(--bg-secondary)] px-4 py-3 flex items-center justify-between">
            <span className="text-sm text-[var(--text-secondary)]">Instalaciones</span>
            <span className="text-sm font-semibold">{isLoading ? '…' : installations.length}</span>
          </div>
          <div className="rounded-lg bg-[var(--bg-secondary)] px-4 py-3 flex items-center justify-between">
            <span className="text-sm text-[var(--text-secondary)]">Versión estable recomendada</span>
            <span className="text-sm font-semibold">{isLoading ? '…' : (latest?.version ?? 'Sin detectar')}</span>
          </div>
          <div className="rounded-lg bg-[var(--bg-secondary)] px-4 py-3 flex items-center justify-between">
            <span className="text-sm text-[var(--text-secondary)]">Compatibilidad</span>
            <span className="text-sm font-semibold">{isLoading ? '…' : (installations.every((i) => i.version.startsWith('1.21')) ? 'Cualificable: 1.21+' : 'Pendiente de verificación')}</span>
          </div>
        </div>
      </Card>

      {/* PLAY */}
      <Card title="Iniciar Minecraft" subtitle="Lanzar una instalación detectada">
        <div className="flex flex-col gap-3">
          <div className="flex items-center justify-center py-4">
            <PlayButton installation={latest ?? installations[0] ?? null} disabled={isLoading} />
          </div>
        </div>
      </Card>

      {/* Status actions */}
      <div className="flex items-center gap-3">
        <button className="text-xs text-[var(--accent)] underline" onClick={handleDiagnostics}>
          Ejecutar diagnóstico
        </button>
        {status && <span className={`text-xs text-[var(--text-secondary)] ml-auto`}>{status.text}</span>}
      </div>

      {/* Last news stub — documentado como pendiente */}
      <Card title="Últimas novedades — LEX CLIENT" subtitle="Ver completo en docs/CHANGELOG.md">
        <ul className="text-sm text-[var(--text-secondary)] list-disc pl-5 space-y-1">
          <li>Fase 1: Investigación y arquitectura.</li>
          <li>Fase 2: Launcher funcional — detección real, navegación real, PLAY real.</li>
          <li>Próximamente: Fase 3 overlay, Fase 4 módulos, Fase 5 integración.</li>
        </ul>
      </Card>
    </div>
  );
}
