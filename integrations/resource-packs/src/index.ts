// LEX CLIENT — Integración resource-packs: Scanner (largo-corto y validación).
// FONTI: Resource Packs validation Rule estándar (schema siguiente), Directorio de resource packs configurable.
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { readFileSyncSafe } from '../../../packages/shared/src/io.js';

/** Nivel de validación de un resource pack (de acuerdo con categorías oficiales de Minecraft). */
export type ValidationLevel = 'malformed' | 'structure-ok' | 'behavior-invalid' | 'behavior-ok';

export interface ScannedResourcePack {
  id: string;                     // nombre del folder (packageId)
  name: string;                   // Nombre configurable a perecer (namespace.json name)
  version: string | null;
  namespace?: string;             // De namespace.json
  description?: string;
  path: string;
  format: 'zip' | 'folder' | 'other';
  validation: ValidationLevel;
  notes: string[];
}

const OFFICIAL_NAMESPACE_KEYS = ['format_version', 'name', 'description', 'namespace'];

function scanNamespaceJson(packDir: string): { namespace?: string; name?: string; description?: string; version: string | null } {
  const candidates = ['namespace.json', 'pack.json'];
  for (const candidate of candidates) {
    const p = join(packDir, candidate);
    try {
      if (!statSync(p).isFile()) continue;
      const content = readFileSyncSafe(p, 'utf8');
      const parsed = JSON.parse(content) as Record<string, unknown>;
      return {
        namespace: typeof parsed['namespace'] === 'string' ? parsed['namespace'] : undefined,
        name: typeof parsed['name'] === 'string' ? parsed['name'] : undefined,
        description: typeof parsed['description'] === 'string' ? parsed['description'] : undefined,
        version: (typeof parsed['format_version'] === 'number' || typeof parsed['format_version'] === 'string')
          ? String(parsed['format_version'])
          : null,
      };
    } catch {
      continue;
    }
  }
  return { version: null };
}

function isZip(file: string): boolean {
  const lower = file.toLowerCase();
  return lower.endsWith('.mcpack') || lower.endsWith('.zip');
}

/** Escaneo de resource packs: lee directorios de resource packs en userData + scans subcarpetas (depth 1). */
export async function scanResourcePacks(roots: string[]): Promise<ScannedResourcePack[]> {
  const packs: ScannedResourcePack[] = [];
  const seen = new Set<string>();
  for (const root of roots) {
    let entries: string[] = [];
    try {
      entries = readdirSync(root);
    } catch {
      continue;
    }
    for (const name of entries) {
      if (seen.has(name)) continue;
      seen.add(name);
      const dir = join(root, name);
      try {
        if (!statSync(dir).isDirectory()) {
          const file = dir;
          if (isZip(file)) {
            packs.push({ id: name, name: name, version: null, path: file, format: 'zip', validation: 'malformed', notes: ['Pack zip: validación de estructura requiere parse JSON dentro del zip (pendiente).'] });
          }
          continue;
        }
        // Folder resource pack: comprueba namespace.json / pack.json.
        const info = scanNamespaceJson(dir);
        if (info.namespace || info.name) {
          packs.push({
            id: name,
            name: info.name ?? name,
            version: info.version,
            namespace: info.namespace,
            description: info.description,
            path: dir,
            format: 'folder',
            validation: 'behavior-ok',
            notes: ['Folder con namespace/name valido; samples true (pending review).'],
          });
        } else {
          packs.push({
            id: name,
            name: name,
            version: info.version,
            path: dir,
            format: 'folder',
            validation: 'malformed',
            notes: ['Sin namespace.json ni pack.json — estructura inválida (pendiente corrección).'],
          });
        }
      } catch {
        continue;
      }
    }
  }
  return packs;
}

export { readFileSyncSafe };
export type { ValidationLevel };
export const OFFICIAL_KEYS = OFFICIAL_NAMESPACE_KEYS;
