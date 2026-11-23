// @lex/shared — Lector de stats del overlay externo (writen por native/overlay-engine).
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { detectEditionByPackageName } from './version.js';
import type { MinecraftEdition } from './types.js';

export interface OverlayStats {
  /** FPS del overlay (EMU del loop D2D propio — no fps del juego). */
  overlayFps: number;
  /** FPS del juego si hook TGA existe (best effort). */
  gameFpsTga: number;
  /** CPS promedio (ring de 4s). */
  cpsAvg: number;
  /** Clicks registrados en el último ciclo. */
  cpsClickCount: number;
  /** Número de keystrokes registrados (ring de 8). */
  keyCount: number;
  /** Timestamp (ms desde epoch). */
  timestamp: number;
  /** Horarius del stats (para invalidación por stale). */
  isStale: boolean;
  /** Age en ms (timestamp ahora - stats timestamp). */
  ageMs: number;
}

export const OVERLAY_STATS_FILE = () => join(process.env['APPDATA'] ?? '', 'lex-client', 'overlay-stats.json');

/** Lector tolerant de JSON (tolerancia a chars inválidos). */
export function parseOverlayStatsJson(json: string): OverlayStats | null {
  try {
    const raw = JSON.parse(json) as Record<string, unknown>;
    const stats: OverlayStats = {
      overlayFps: typeof raw['overlayFps'] === 'number' ? raw['overlayFps'] : 0,
      gameFpsTga: typeof raw['gameFpsTga'] === 'number' ? raw['gameFpsTga'] : -1,
      cpsAvg: typeof raw['cpsAvg'] === 'number' ? raw['cpsAvg'] : 0,
      cpsClickCount: typeof raw['cpsClickCount'] === 'number' ? raw['cpsClickCount'] : 0,
      keyCount: typeof raw['keyCount'] === 'number' ? raw['keyCount'] : 0,
      timestamp: typeof raw['timestamp'] === 'number' ? raw['timestamp'] : 0,
      isStale: false,
      ageMs: 0,
    };
    stats.ageMs = Math.max(0, Date.now() - stats.timestamp);
    stats.isStale = stats.ageMs > 1500;
    return stats;
  } catch {
    return null;
  }
}

export function readOverlayStats(path: string = OVERLAY_STATS_FILE()): OverlayStats | null {
  try {
    return parseOverlayStatsJson(readFileSync(path, 'utf8'));
  } catch {
    return null;
  }
}

/** Categoria del timestamp de una instalación (utilitário, temporal para módulos). */
export function detectEditionFromName(pkg: string): MinecraftEdition {
  return detectEditionByPackageName(pkg);
}
