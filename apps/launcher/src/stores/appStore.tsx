// LEX CLIENT — Estado global del launcher (React context + store simple).
import { createContext, useContext, useReducer } from 'react';
import type { MinecraftInstallation } from '@lex/shared';

export type Screen = 'home' | 'versions' | 'mods' | 'cosmetics' | 'settings';

interface AppState {
  current: Screen;
  theme: 'dark' | 'light';
  installations: MinecraftInstallation[];
  latest: MinecraftInstallation | null;
  loading: { detect: boolean; launch: boolean; diagnostics: boolean; updates: boolean };
  status: { text: string; tone: 'ok' | 'warning' | 'error' | 'muted' } | null;
  actionErrors: Record<string, string | null>;
}

type SetStatus = (status: AppState['status']) => void;
type SetTheme = (theme: 'dark' | 'light') => void;
type SetInstallations = (installations: MinecraftInstallation[], latest: MinecraftInstallation | null) => void;

type Action =
  | { type: 'set-current'; screen: Screen }
  | { type: 'set-theme'; theme: 'dark' | 'light' }
  | { type: 'set-installations'; installations: MinecraftInstallation[]; latest: MinecraftInstallation | null }
  | { type: 'start'; key: string }
  | { type: 'finish'; key: string }
  | { type: 'set-status'; status: AppState['status'] }
  | { type: 'set-action-error'; key: string; message: string | null }
  | { type: 'reset-errors' };

const initial: AppState = {
  current: 'home',
  theme: 'dark',
  installations: [],
  latest: null,
  loading: { detect: false, launch: false, diagnostics: false, updates: false },
  status: null,
  actionErrors: {},
};

function reducer(state: AppState, action: Action): AppState {
  switch (action.type) {
    case 'set-current':
      return { ...state, current: action.screen };
    case 'set-theme':
      return { ...state, theme: action.theme };
    case 'set-installations':
      return { ...state, installations: action.installations, latest: action.latest };
    case 'start':
      return { ...state, loading: { ...state.loading, [action.key]: true } };
    case 'finish':
      return { ...state, loading: { ...state.loading, [action.key]: false } };
    case 'set-status':
      return { ...state, status: action.status };
    case 'set-action-error':
      return { ...state, actionErrors: { ...state.actionErrors, [action.key]: action.message } };
    case 'reset-errors':
      return { ...state, actionErrors: {} };
    default:
      return state;
  }
}

const StoreContext = createContext<AppState | null>(null);
const DispatchContext = createContext<AppDispatch | null>(null);

export function StoreProvider({ children }: { children: React.ReactNode }) {
  const [state, dispatch] = useReducer(reducer, initial);
  return (
    <StoreContext.Provider value={state}>
      <DispatchContext.Provider value={dispatch}>{children}</DispatchContext.Provider>
    </StoreContext.Provider>
  );
}

export function useApp(): AppState {
  const ctx = useContext(StoreContext);
  if (!ctx) throw new Error('useApp() debe usarse dentro de StoreProvider');
  return ctx;
}

export function useAppDispatch(): AppDispatch {
  const ctx = useContext(DispatchContext);
  if (!ctx) throw new Error('useAppDispatch() debe usarse dentro de StoreProvider');
  return ctx;
}

export type AppDispatch = (action: Action) => void;

/** Convenientes setters con tipos fuertes. */
export function useAppHelpers() {
  const dispatch = useAppDispatch();
  const setStatus: SetStatus = (status) => dispatch({ type: 'set-status', status });
  const setTheme: SetTheme = (theme) => dispatch({ type: 'set-theme', theme });
  const setInstallations: SetInstallations = (installations, latest) => dispatch({ type: 'set-installations', installations, latest });
  const start = (key: string) => dispatch({ type: 'start', key });
  const finish = (key: string) => dispatch({ type: 'finish', key });
  return { setStatus, setTheme, setInstallations, start, finish };
}
