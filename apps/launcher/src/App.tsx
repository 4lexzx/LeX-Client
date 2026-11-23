import React from 'react';
import { Home } from './screens/Home';
import { VersionManager } from './screens/VersionManager';
import { Mods } from './screens/Mods';
import { Cosmetics } from './screens/Cosmetics';
import { Settings } from './screens/Settings';
import { Sidebar } from './components/Sidebar';
import { StoreProvider, useApp, useAppDispatch } from './stores/appStore';

type Screen = import('./stores/appStore').Screen;

export { StoreProvider };

export function App() {
  const { current, theme } = useApp();
  const dispatch = useAppDispatch();

  const screens: Record<Screen, React.ReactElement> = {
    home: <Home />,
    versions: <VersionManager />,
    mods: <Mods />,
    cosmetics: <Cosmetics />,
    settings: <Settings />,
  };

  // Observa cambios de tema para datos de UI.
  React.useEffect(() => {
    document.documentElement.dataset.theme = theme;
  }, [theme]);

  return (
    <div className="flex h-screen overflow-hidden">
      <div className="fixed inset-0 -z-10 overflow-hidden pointer-events-none">
        <div className="absolute -top-40 -left-40 w-[600px] h-[600px] bg-[var(--accent)] opacity-[0.03] blur-[120px] rounded-full" />
        <div className="absolute -bottom-60 right-0 w-[700px] h-[700px] bg-[var(--accent)] opacity-[0.02] blur-[140px] rounded-full" />
      </div>
      <Sidebar current={current} onNav={(s) => dispatch({ type: 'set-current', screen: s })} />
      <main className="flex-1 overflow-y-auto min-h-0">{screens[current]}</main>
    </div>
  );
}
