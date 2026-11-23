// @lex/shared — tipos compartidos para todo LEX CLIENT (TS estricto, ESM).

export type MinecraftEdition = 'retail' | 'preview' | 'education';

export interface MinecraftInstallation {
  /** Id único interno: GUID del paquete (retail/preview/education). */
  id: string;
  /** Nombre del paquete de Windows, e.g. Microsoft.minecraftUWP_1.21.130.2_x64__8wekyb3d8bbbe. */
  packageId: string;
  /** Nombre de mostrar, e.g. Minecraft for Windows. */
  displayName: string;
  /** Edición del juego. */
  edition: MinecraftEdition;
  /** Versión del paquete normalizada, e.g. 1.21.130.2. */
  version: string;
  /** Path al ejecutable de juego (resuelto; null si no se encuentra). */
  executable: string | null;
  /** Directorio de instalación detectada. */
  installLocation: string;
  /** Cómo se detectó (delegado al actualizador? no) */
  source: 'powershell-appxpackage' | 'filesystem-manifest' | 'gamedk-xboxgames';
  /** Es versión preview? */
  isPreview: boolean;
  /** Fecha de detección (ISO 8601). */
  detectedAt: string;
}

export interface MinecraftUpdateAvailable {
  installationId: string;
  current: string;
  latest: string;
  notes: string;
  channel: 'retail' | 'preview';
}

export type ModuleCategory = 'hud' | 'visual' | 'gameplay' | 'utilities';

export interface ModuleMeta {
  /** Id único, camelCase. */
  id: string;
  name: string;
  description: string;
  category: ModuleCategory;
  /** 'reserved' => los datos no están disponibles (fuente honesta) */
  dataSource: 'external-overlay' | 'game-memory-v1' | 'game-memory-v2' | 'script-api-v1' | 'local-only' | 'reserved';
  /** Versión mínima del juego donde el adapter está disponible. */
  requiresMinVersion?: string;
  /** Versiones a las que aplica (si usa memoria del juego). */
  compatibleWith?: string[];
  /** Atajos de teclado configurables. */
  keybinds?: string[];
  /** Componente que renderiza el módulo en el overlay (para el external engine). */
  overlayRenderer?: 'hud-fps' | 'hud-cps' | 'hud-keystrokes' | 'hud-clock' | string;
}

export interface ModuleRuntimeState {
  moduleId: string;
  enabled: boolean;
  position?: { x: number; y: number };
  scale?: number;
  visible?: boolean;
  customSettings?: Record<string, unknown>;
}

export interface ConfigSnapshot {
  version: number;
  timestamp: string;
  name?: string;
  mcVersion: string | null;
  modules: Record<string, ModuleRuntimeState>;
  hudLayout: Array<{ moduleId: string; x: number; y: number; width: number; height: number; scale: number }>;
}

export type IpcChannel =
  | 'bedrock:detect'
  | 'game:launch'
  | 'game:window'
  | 'overlay:show'
  | 'overlay:hide'
  | 'overlay:click-through'
  | 'profiles:list'
  | 'profiles:apply'
  | 'diagnostics:run'
  | 'updates:check';

export interface Diagnostic {
  id: string;
  level: 'ok' | 'warning' | 'error';
  message: string;
  details?: Record<string, unknown>;
  fixable?: boolean;
  suggest?: string;
}

export interface DiagnosticReport {
  timestamp: string;
  diagnostics: Diagnostic[];
  platform: { os: string; arch: string; node: string };
}
