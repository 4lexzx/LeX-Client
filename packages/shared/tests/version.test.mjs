// LEX CLIENT — Tests unitarios de detección de versiones (packages/shared, TS con tipos).
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { normalizeInternalVersion, detectEditionByPackageName, extractVersionFromFolderName, detectInstallationFromPackageEntry } from '../src/version.ts';

describe('normalizeInternalVersion', () => {
  it('acepta versiones puros', () => {
    assert.equal(normalizeInternalVersion('1.21.130.2'), '1.21.130.2');
    assert.equal(normalizeInternalVersion('1.26.30.5'), '1.26.30.5');
    assert.equal(normalizeInternalVersion('1.21.120'), '1.21.120');
  });
  it('rechaza versiones invalidas', () => {
    assert.equal(normalizeInternalVersion(null), null);
    assert.equal(normalizeInternalVersion(''), null);
    assert.equal(normalizeInternalVersion('abc'), null);
    assert.equal(normalizeInternalVersion('1..2'), null);
  });
  it('trimiza espacios', () => {
    assert.equal(normalizeInternalVersion('  1.21.0.0  '), '1.21.0.0');
  });
});

describe('detectEditionByPackageName', () => {
  it('identifica retail/preview/education', () => {
    assert.equal(detectEditionByPackageName('Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe'), 'retail');
    assert.equal(detectEditionByPackageName('Microsoft.MinecraftWindowsBeta_1.21.120.21_x64__8wekyb3d8bbbe'), 'preview');
    assert.equal(detectEditionByPackageName('Microsoft.MinecraftEducationEdition_1.20.130.0_x64__8wekyb3d8bbbe'), 'education');
  });
  it('defaultea a retail para desconocidos', () => {
    assert.equal(detectEditionByPackageName('Random_1.0.0.0_x64__8wekyb3d8bbbe'), 'retail');
  });
});

describe('extractVersionFromFolderName', () => {
  it('saca la versión del folder', () => {
    assert.equal(extractVersionFromFolderName('Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe'), '1.21.130.2');
  });
});

describe('detectInstallationFromPackageEntry', () => {
  it('crea instalación desde entrada raw', () => {
    const inst = detectInstallationFromPackageEntry({ name: 'Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe', version: '1.21.130.2' });
    assert.ok(inst);
    assert.equal(inst.displayName, 'Minecraft for Windows');
    assert.equal(inst.source, 'powershell-appxpackage');
  });
  it('ignora entradas sin versión válida', () => {
    assert.equal(detectInstallationFromPackageEntry({ name: 'X', version: '' }), null);
  });
});
