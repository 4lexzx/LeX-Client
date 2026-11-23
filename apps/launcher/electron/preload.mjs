// LEX CLIENT — Preload. Bridge minimal entre renderer y main (IPC tipado).
import { contextBridge, ipcRenderer } from 'electron';

const invoke = (channel, ...args) => ipcRenderer.invoke(channel, ...args);

contextBridge.exposeInMainWorld('lex', {
  ipc: {
    detect: () => invoke('bedrock:detect'),
    launch: (installationId) => invoke('game:launch', installationId),
    windowInfo: () => invoke('game:window'),
    catalog: () => invoke('modules:catalog'),
    modulesGet: () => invoke('modules:get'),
    modulesSetState: (state) => invoke('modules:setState', state),
    profilesList: () => invoke('profiles:list'),
    profilesApply: (profileId) => invoke('profiles:apply', profileId),
    diagnostics: () => invoke('diagnostics:run'),
    diagnosticsApplyFix: (id) => invoke('diagnostics:apply-fix', id),
    updatesCheck: () => invoke('updates:check'),
    overlaySetState: (state) => invoke('overlay:set-state', state),
    cosmeticsGet: () => invoke('cosmetics:get'),
    cosmeticsSave: (items) => invoke('cosmetics:save', items),
  },
  on: (event, listener) => {
    const wrapped = (_e, ...args) => listener(...args);
    ipcRenderer.on(event, wrapped);
    return () => ipcRenderer.removeListener(event, wrapped);
  },
  auth: { loginStatus: () => invoke('auth:status'), logout: () => invoke('auth:logout') },
});
