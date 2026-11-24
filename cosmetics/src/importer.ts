// LEX CLIENT — Importación y validación de cosméticos propios (schema validado antes de aplicar).
import type { Cosmetic, ImportResult, ImportValidationIssue } from './types.js';

/** Validación conocida: JSON parseable, campos requeridos y metadatos de animación coherentes. */
function validateEntry(raw: unknown, index: number): Cosmetic {
  if (typeof raw !== 'object' || raw === null) throw new Error(`item ${index}: no es un objeto`);
  const r = raw as Record<string, unknown>;
  if (typeof r['id'] !== 'string' || r['id'] === '') throw new Error(`item ${index}: id requerido`);
  if (typeof r['name'] !== 'string' || r['name'] === '') throw new Error(`item ${index}: name requerido`);
  const type = r['type'];
  if (type !== 'local' && type !== 'resource-pack' && type !== 'import') throw new Error(`item ${index}: type invalido (esperado local|resource-pack|import)`);
  const source = r['source'];
  if (source !== 'private' && source !== 'shared') throw new Error(`item ${index}: source invalido (esperado private|shared)`);
  const out: Cosmetic = { id: r['id'], name: r['name'], type, source };

  if (typeof r['version'] === 'string') out.version = r['version'];
  if (typeof r['description'] === 'string') out.description = r['description'];
  if (typeof r['hash'] === 'string') out.hash = r['hash'];
  if (typeof r['layers'] === 'number' && Number.isFinite(r['layers']) && r['layers'] >= 0) out.layers = Math.floor(r['layers']);

  if (typeof r['preview'] === 'string' && r['preview']) out.preview = basename(r['preview']);
  if (typeof r['atlas'] === 'string' && r['atlas']) out.atlas = basename(r['atlas']);
  if (typeof r['animated'] === 'boolean') out.animated = r['animated'];

  if (typeof r['frames'] === 'number' && Number.isInteger(r['frames']) && r['frames'] > 0 && r['frames'] <= 256) {
    out.frames = r['frames'];
  }
  if (typeof r['frameMs'] === 'number' && Number.isFinite(r['frameMs']) && r['frameMs'] >= 16 && r['frameMs'] <= 5000) {
    out.frameMs = Math.floor(r['frameMs']);
  }
  if (r['active'] === true || r['active'] === false) out.active = r['active'];
  if (out.animated === undefined && (out.frames !== undefined || out.atlas !== undefined)) out.animated = true;
  return out;
}

/** Sólo nombre de archivo: evita path traversal desde un JSON importado. */
function basename(value: string): string {
  return value.replace(/\\/g, '/').split('/').filter(Boolean).pop() ?? '';
}

/** Dedupe por id: la última entrada gana (permite actualizar un cosmético importado). */
export function dedupeCosmetics(items: Cosmetic[]): Cosmetic[] {
  const byId = new Map<string, Cosmetic>();
  for (const item of items) byId.set(item.id, item);
  return [...byId.values()];
}

/** Parsea una lista de cosméticos desde string JSON (mocks y tooling). */
export function parseCosmeticList(json: string): Cosmetic[] {
  const parsed = JSON.parse(json) as unknown;
  if (!Array.isArray(parsed)) throw new Error('lista de cosméticos debe ser un array');
  return dedupeCosmetics(parsed.map((p, i) => validateEntry(p, i)));
}

/** Importa desde string con handler de rechazo (fallible, resolvible). */
export function importCosmeticsFromString(content: string): ImportResult {
  const accepted: Cosmetic[] = [];
  const rejected: ImportValidationIssue[] = [];
  try {
    const parsed = JSON.parse(content) as unknown;
    if (!Array.isArray(parsed)) throw new Error('la lista de cosméticos debe ser un array');
    for (let i = 0; i < parsed.length; i++) {
      try {
        accepted.push(validateEntry(parsed[i], i));
      } catch (e) {
        rejected.push({ itemId: `item-${i}`, level: 'error', message: e instanceof Error ? e.message : String(e) });
      }
    }
  } catch (e) {
    rejected.push({ itemId: '-', level: 'error', message: `JSON inválido: ${e instanceof Error ? e.message : String(e)}` });
  }
  return { accepted: dedupeCosmetics(accepted), rejected };
}

/** Lee un archivo de cosmético JSON. Renderer: usa importCosmeticsFromString con f.text(). */
export function readCosmeticFile(path: string): ImportResult {
  return { accepted: [], rejected: [{ itemId: '-', level: 'error', message: `lectura por path no disponible en renderer (${path}); usa importCosmeticsFromString con texto del archivo` }] };
}
