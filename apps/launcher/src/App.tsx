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

  React.useEffect(() => {
    document.documentElement.dataset.theme = theme;
  }, [theme]);

  return (
    <div className="lex-app">
      <div className="lex-ambient" aria-hidden="true">
        <span className="lex-ambient-a" />
        <span className="lex-ambient-b" />
        <span className="lex-ambient-c" />
        <span className="lex-ambient-grid" />
      </div>
      <Sidebar current={current} onNav={(s) => dispatch({ type: 'set-current', screen: s })} />
      <main className="lex-main" id="lex-main">{screens[current]}</main>
    </div>
  );
}
