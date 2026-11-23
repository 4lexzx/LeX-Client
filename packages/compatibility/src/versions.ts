// @lex/compatibility — Adaptadores por versión de Minecraft Bedrock.
// Honestidad: cada versión PUBLICADA en la matriz debe tener been heurístico o comprobado.
// Variables con `verifiedBy` se marcan como "pedir notificar de Actualización en Fase 5".

import type { MinecraftInstallation, MinecraftEdition } from '@lex/shared';
import { normalizeInternalVersion } from '@lex/shared';

export interface VersionRange {
  /** Semver mínimo aplicable, excluido: "1.21.120.2" -display đầu tiên. */
  min: string;
  /** Semver máximo aplicable (opcional, para rangos cerrados). */
  max?: string;
  /** Tipo de cliente que dio verifiedBy. */
  edition: MinecraftEdition;
}

export interface VersionAdapter {
  id: string;
  ranges: VersionRange[];
  /** Adaptador ruteado internamente. */
  source: 'game-memory-v1' | 'game-memory-v2';
  supported: boolean;
  notes: string;
}

function normalize(v: string): string {
  return normalizeInternalVersion(v) ?? v;
}

function cmp(a: string, b: string): number {
  const pa = a.split('.').map((n) => parseInt(n, 10) || 0);
  const pb = b.split('.').map((n) => parseInt(n, 10) || 0);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    const d = (pa[i] ?? 0) - (pb[i] ?? 0);
    if (d !== 0) return d;
  }
  return 0;
}

/** ¿La instalación dada está dentro de algún rango del adapter dado? */
export function isAdapterCompatible(adapter: VersionAdapter, installation: MinecraftInstallation): boolean {
  const v = normalize(installation.version);
  if (adapter.edition && installation.edition !== adapter.edition) return false;
  return adapter.ranges.some((r) => {
    const inMin = cmp(v, normalize(r.min)) >= 0;
    if (!inMin) return false;
    if (r.max && cmp(v, normalize(r.max)) > 0) return false;
    return true;
  });
}

/** Adapter seleccionado para una instalación (o null si no hay compatible). */
export function selectAdapter(installations: MinecraftInstallation[]): { installation: MinecraftInstallation; adapter: VersionAdapter } | null {
  const sorted = [...installations].sort((a, b) => cmp(normalize(b.version), normalize(a.version)));
  for (const installation of sorted) {
    for (const adapter of ADAPTERS) {
      if (!adapter.supported) continue;
      if (isAdapterCompatible(adapter, installation)) return { installation, adapter };
    }
  }
  return null;
}

/** Adaptadores v1 de memoria de juego. Versiones reales verificables se marcan. */
const ADAPTERS: VersionAdapter[] = [
  {
    id: 'adapter-v1',
    source: 'game-memory-v1',
    supported: true,
    edition: 'retail',
    ranges: [{ min: '1.21.120.2', max: '1.26.29', edition: 'retail' }],
    notes: 'Adaptador v1 basado en heurística de rango; requiere notificar FromPhase5 con pruebas reales (Get-AppxPackage). Sin guarantee function exposure, verificar en Fase 5.',
  },
  {
    id: 'adapter-v2',
    source: 'game-memory-v2',
    supported: true,
    edition: 'retail',
    ranges: [{ min: '1.26.30.5', edition: 'retail' }],
    notes: 'Adaptador v2 (disponible per changelog Flarial 2026-06, pero pendiente verificación en Fase 5 con detección real).',
  },
];

/** Comprobación de la matriz real (usada por los tests de Fase 5). */
export function buildCompatibilityMatrix(installations: MinecraftInstallation[]): Array<{ installation: MinecraftInstallation; adapter: VersionAdapter | null }> {
  return installations.map((installation) => ({
    installation,
    adapter: ADAPTERS.find((a) => isAdapterCompatible(a, installation)) ?? null,
  }));
}
