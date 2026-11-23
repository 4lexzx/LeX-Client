// LEX CLIENT — Tests unitarios de detección (electron/detection.mjs).
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { normalizeInternalVersion, detectEditionByPackageName, extractVersionFromFolderName, detectInstallationFromPackageEntry } from '../electron/detection.mjs';

describe('normalizeInternalVersion', () => {
  it('acepta versiones puros', () => {
    assert.equal(normalizeInternalVersion('1.21.130.2'), '1.21.130.2');
    assert.equal(normalizeInternalVersion('1.26.30.5'), '1.26.30.5');
  });
  it('rechaza inválidas', () => {
    assert.equal(normalizeInternalVersion(null), null);
    assert.equal(normalizeInternalVersion('abc'), null);
  });
});

describe('detectEditionByPackageName', () => {
  it('retail por defecto, preview/education detectadas', () => {
    assert.equal(detectEditionByPackageName('Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe'), 'retail');
    assert.equal(detectEditionByPackageName('Microsoft.MinecraftWindowsBeta_1.21.120.21_x64__8wekyb3d8bbbe'), 'preview');
    assert.equal(detectEditionByPackageName('Microsoft.MinecraftEducationEdition_1.20.130.0_x64__8wekyb3d8bbbe'), 'education');
  });
});

describe('extractVersionFromFolderName', () => {
  it('extrae la versión', () => {
    assert.equal(extractVersionFromFolderName('Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe'), '1.21.130.2');
  });
});

describe('detectInstallationFromPackageEntry', () => {
  it('crea instalación', () => {
    const inst = detectInstallationFromPackageEntry({ name: 'Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe', version: '1.21.130.2' });
    assert.ok(inst);
    assert.equal(inst.edition, 'retail');
    assert.equal(inst.isPreview, false);
    assert.equal(inst.source, 'powershell-appxpackage');
  });
  it('rechaza entradas sin versión válida', () => {
    assert.equal(detectInstallationFromPackageEntry({ name: 'X', version: '' }), null);
  });
});
