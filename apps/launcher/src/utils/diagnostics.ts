// LEX CLIENT — Utilidades de diagnóstico del launcher.
import type { DiagnosticReport, Diagnostic } from '@lex/shared';

export async function runDiagnostics(): Promise<DiagnosticReport> {
  const report = (await window.lex.ipc.diagnostics()) as DiagnosticReport;
  // Marshalling: sorted por severidad para mostrar mejor.
  report.diagnostics.sort((a, b) => severity(a) - severity(b));
  return report;
}

function severity(d: Diagnostic): number {
  return d.level === 'error' ? 0 : d.level === 'warning' ? 1 : 2;
}

export function applyFix(id: string): Promise<{ applied: boolean; id: string; note?: string; url?: string }> {
  return window.lex.ipc.diagnosticsApplyFix(id);
}
