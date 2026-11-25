const { contextBridge, ipcRenderer } = require('electron');

const call = (channel) => (...args) => ipcRenderer.invoke(channel, ...args);

contextBridge.exposeInMainWorld('lex', {
  status: call('status'),
  play: call('play'),
  modules: call('modules'),
  setModule: call('modules:set'),
  loadout: call('loadout'),
  saveLoadout: call('loadout:save'),
  texture: call('texture'),
  skin: call('skin'),
  optimizer: call('optimizer'),
  optimize: call('optimizer:apply'),
  uninstall: call('uninstall'),
  openFolder: call('folder'),
});
