// LEX CLIENT — Detección de Minecraft: ejecuta IPC al montar. Idempotente (activación única).
import { useEffect } from 'react';
import { useAppDispatch } from '../stores/appStore';
import type { AppDispatch } from '../stores/appStore';

export type AppDispatchAlias = AppDispatch;

/** Dispatch inicial de detección: mount-once, y puede refrescar después. */
export function useDetectOnMount() {
  const dispatch = useAppDispatch();
  useEffect(() => {
    let cancelled = false;
    const run = async () => {
      dispatch({ type: 'start', key: 'detect' });
      try {
        const r = await window.lex.ipc.detect();
        if (!cancelled) dispatch({ type: 'set-installations', installations: r.installations, latest: r.latest });
      } catch (e) {
        console.error('[LEX] detect error:', e);
      } finally {
        if (!cancelled) dispatch({ type: 'finish', key: 'detect' });
      }
    };
    void run();
    return () => {
      cancelled = true;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [dispatch]);
}

export type { AppDispatch };
