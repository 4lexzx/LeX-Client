// LEX CLIENT — Importación y validación de cosméticos propios (schema nuevo: honesto, validar antes).
import type { Cosmetic, ImportResult, ImportValidationIssue } from './types.js';

/** Validación conocida: JSON parseable y campos requeridos presentes (id, name, type, source validos). */
function validateEntry(raw: unknown, index: number): Cosmetic {
  if (typeof raw !== 'object' || raw === null) throw new Error(`item ${index}: no es un objeto`);
  const r = raw as Record<string, unknown>;
  if (typeof r['id'] !== 'string' || r['id'] === '') throw new Error(`item ${index}: id requerido`);
  if (typeof r['name'] !== 'string' || r['name'] === '') throw new Error(`item ${index}: name requerido`);
  const type = r['type'];
  if (type !== 'local' && type !== 'resource-pack' && type !== 'import') throw new Error(`item ${index}: type invalido (esperado local|resource-pack|import)`);
  const source = r['source'];
  if (source !== 'private' && source !== 'shared') throw new Error(`item ${index}: source invalido (esperado private|shared)`);
  const out: Cosmetic = {
    id: r['id'],
    name: r['name'],
    type,
    source,
  };
  if (typeof r['version'] === 'string') out.version = r['version'];
  if (typeof r['description'] === 'string') out.description = r['description'];
  if (typeof r['layers'] === 'number') out.layers = r['layers'];
  return out;
}

/** Parsea una lista de cosméticos desde string JSON (mocks y tooling). */
export function parseCosmeticList(json: string): Cosmetic[] {
  const parsed = JSON.parse(json) as unknown;
  if (!Array.isArray(parsed)) throw new Error('lista de cosméticos debe ser un array');
  return parsed.map((p) => validateEntry(p, 0));
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
  return { accepted, rejected };
}

/**Lee un archivo de cosmético JSON. Renderer: usa importCosmeticsFromString con f.text(); tooling/Node ejecuta lectura real. */
export function readCosmeticFile(path: string): ImportResult {
  // En renderer no hay fs; aquí devolvemos un resultado honesto. Para tooling/Node, importCosmeticsFromString con texto real.
  return { accepted: [], rejected: [{ itemId: '-', level: 'error', message: `lectura por path no disponible en renderer (${path}); usa importCosmeticsFromString con texto del archivo` }] };
}
