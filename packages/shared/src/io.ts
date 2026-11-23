// LEX CLIENT — IO utils: lectura segura de archivos (sin throw) — portable a C++/TODO.
import { readFileSync } from 'node:fs';

export function readFileSyncSafe(path: string, encoding: BufferEncoding = 'utf8'): string | null {
  try {
    return readFileSync(path, encoding);
  } catch {
    return null;
  }
}

export function existsSafe(path: string): boolean {
  try {
    readFileSync(path);
    return true;
  } catch {
    return false;
  }
}
