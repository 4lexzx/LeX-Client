// LEX CLIENT — Badge de estado de la detección de Minecraft.
import { Badge } from '@lex/ui-components';
import type { MinecraftInstallation } from '@lex/shared';

export function DetectionStatus({ installations, isLoading, hasError }: { installations: MinecraftInstallation[]; isLoading: boolean; hasError?: boolean }) {
  if (isLoading) return <Badge variant="accent">Detectando…</Badge>;
  if (hasError) return <Badge variant="error">Error en la detección</Badge>;
  if (installations.length === 0) return <Badge variant="warning">No se encontró Minecraft</Badge>;
  return <Badge variant="success">Minecraft: {installations.length} instalación(es)</Badge>;
}
