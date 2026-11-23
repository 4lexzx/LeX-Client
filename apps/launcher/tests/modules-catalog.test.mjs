// LEX CLIENT — Tests de integridad del catálogo de módulos (modules/catalog.mjs).
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { MODULES_CATALOG } from '../../../modules/catalog.mjs';

const VALID_CATEGORIES = new Set(['hud', 'visual', 'gameplay', 'utilities']);
const VALID_DATA_SOURCES = new Set(['external-overlay', 'game-memory-v1', 'game-memory-v2', 'script-api-v1', 'local-only', 'reserved']);

describe('MODULES_CATALOG', () => {
  it('tiene módulos', () => {
    assert.ok(MODULES_CATALOG.length >= 3, 'debería tener al menos 3 módulos por ahora');
  });

  it('cada entrada tiene metadatos únicos y válidos', () => {
    const ids = new Set();
    for (const m of MODULES_CATALOG) {
      assert.equal(typeof m.id, 'string', 'id string');
      assert.ok(m.id && !ids.has(m.id), `id único ${m.id}`);
      ids.add(m.id);
      assert.ok(m.name, 'name');
      assert.ok(m.description, 'description');
      assert.ok(VALID_CATEGORIES.has(m.category), `categoria válida ${m.id}`);
      assert.ok(VALID_DATA_SOURCES.has(m.dataSource), `dataSource válida ${m.id}`);
      if (m.requiresMinVersion) assert.match(m.requiresMinVersion, /^\d+(\.\d+)+$/);
    }
  });

  it('módulos externos van en el overlayEngine y reserved marca fuente honesta', () => {
    for (const m of MODULES_CATALOG) {
      if (m.dataSource === 'external-overlay') assert.ok(m.overlayRenderer, `${m.id} debería tener overlayRenderer`);
      if (m.dataSource === 'reserved') assert.ok(!m.overlayRenderer, `${m.id} reservado no tiene render funcional`);
    }
  });
});
