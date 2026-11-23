// LEX CLIENT — Botón PLAY — Real: lanza Minecraft.Windows.exe vía main (spawn por IPC).
import { useState } from 'react';
import { Button, Badge } from '@lex/ui-components';
import type { MinecraftInstallation } from '@lex/shared';
import { useAppHelpers } from '../stores/appStore';

export function PlayButton({ installation, disabled = false }: { installation: MinecraftInstallation | null; disabled?: boolean }) {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<{ message: string } | null>(null);
  const { setStatus } = useAppHelpers();

  async function handlePlay() {
    if (!installation?.id || loading) return;
    setLoading(true);
    setError(null);
    try {
      const r = await window.lex.ipc.launch(installation.id);
      if (r.ok) {
        setStatus({ text: `Minecraft ${r.displayName ?? ''} ${r.version ?? ''}${r.pid ? ` — PID: ${r.pid}` : ''}`, tone: 'ok' });
      } else {
        const message = r.error ? `Error: ${r.error}` : 'Error al iniciar Minecraft.';
        setError({ message });
        setStatus({ text: 'No se pudo lanzar Minecraft.', tone: 'error' });
      }
    } catch {
      setError({ message: 'Error de IPC al lanzar.' });
      setStatus({ text: 'Error de IPC al lanzar Minecraft.', tone: 'error' });
    } finally {
      setLoading(false);
    }
  }

  const locked = !installation || disabled || loading;

  return (
    <div className="flex items-center gap-2 flex-wrap">
      <Button variant="primary" size="lg" onClick={handlePlay} disabled={locked} aria-label="Jugar a Minecraft">
        {loading ? 'Iniciando…' : 'JUGAR'}
      </Button>
      {error && <Badge variant="error">{error.message}</Badge>}
      {installation && (
        <span className="text-xs text-[var(--text-muted)] flex items-center gap-1.5">
          <span className={`status-dot ${installation.isPreview ? 'warning' : 'ok'}`} />
          {installation.displayName} — {installation.version} ({installation.edition})
        </span>
      )}
    </div>
  );
}
