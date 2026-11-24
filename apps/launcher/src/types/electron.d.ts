// LEX CLIENT — Tipos de la API que preload expone a la renderer.
import type { MinecraftInstallation, ModuleMeta } from '@lex/shared';
import type { Cosmetic } from '@lex/cosmetics';

declare global {
  interface Window {
    lex: {
      ipc: {
        detect(): Promise<{ installations: MinecraftInstallation[]; latest: MinecraftInstallation | null }>;
        launch(id: string): Promise<{ ok: boolean; pid?: number; error?: string; displayName?: string | null; version?: string }>;
        windowInfo(): Promise<{ installed: Array<Record<string, unknown>> }>;
        catalog(): Promise<{ modules: ModuleMeta[] }>;
        modulesGet(): Promise<Record<string, { enabled: boolean; updatedAt?: string }>>;
        modulesSetState(state: { id: string; enabled: boolean }): Promise<{ ok: boolean; id: string; enabled: boolean }>;
        profilesList(): Promise<unknown[]>;
        profilesApply(id: string): Promise<{ ok: boolean; id?: string; error?: string }>;
        diagnostics(): Promise<unknown>;
        diagnosticsApplyFix(id: string): Promise<{ applied: boolean; id: string; note?: string; url?: string }>;
        updatesCheck(): Promise<{ version: string; notes: string; changed: boolean; launcherVersion?: string }>;
        overlaySetState(state: Record<string, unknown>): Promise<{ ok: boolean }>;
        cosmeticsGet(): Promise<Cosmetic[]>;
        cosmeticsSave(items: Cosmetic[]): Promise<{ ok: boolean; count: number }>;
        cosmeticsPreview(file: string): Promise<{ dataUrl: string | null }>;
        cosmeticsOpenPack(): Promise<{ ok: boolean; error?: string }>;
        logoGet(): Promise<{ dataUrl: string | null }>;
        logoPick(): Promise<{ ok: boolean }>;
      };
      on(event: string, listener: (payload?: unknown) => void): () => void;
      auth: {
        loginStatus(): Promise<{ signedIn: boolean; account: string | null; note: string }>;
        logout(): Promise<{ ok: boolean }>;
      };
    };
  }
}

export {};
