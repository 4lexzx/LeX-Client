import { useEffect } from 'react';

/** Ejecuta una vez al montar (intenta hacer async + manejo de error). */
export function useEffectOnce(effect: () => void | Promise<void>, deps: unknown[] = []) {
  useEffect(() => {
    const r = effect();
    if (r instanceof Promise) {
      r.catch((e) => console.error('[LEX] effect:', e));
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [...deps]);
}
