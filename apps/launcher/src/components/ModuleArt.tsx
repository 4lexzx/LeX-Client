// LEX CLIENT — Sistema de iconografía de módulos.
// Familia propia: glifos vectoriales de 32x32, trazo consistente, relleno duotono
// y un tile de fondo común. No se usan assets externos ni se copian iconos de otros clientes.
// El overlay GDI dibuja los mismos conceptos en engine.cpp (drawModuleGlyph).

import React from 'react';

const C = 'currentColor';

function Fill({ children, opacity = 1 }: { children: React.ReactNode; opacity?: number | string }) {
  return <g fill={C} stroke="none" opacity={opacity}>{children}</g>;
}

function KeyCap({ x, y, label }: { x: number; y: number; label: string }) {
  return (
    <g>
      <rect x={x} y={y} width="8" height="8" rx="1.6" />
      <text x={x + 4} y={y + 5.7} fontSize="5" fontFamily="monospace" fontWeight="700" textAnchor="middle" stroke="none" fill={C}>{label}</text>
    </g>
  );
}

function glyphFor(id: string): React.ReactNode {
  switch (id) {
    // ---------- HUD ----------
    case 'hud-fps':
      return (
        <>
          <rect x="4.5" y="6" width="23" height="20" rx="3.2" />
          <path d="M8 21 L12 15.5 L15.5 18 L20 10.5 L24 13.5" />
          <Fill><circle cx="24" cy="13.5" r="1.6" /></Fill>
          <path d="M8 24.5 H16" opacity="0.45" />
        </>
      );
    case 'hud-cps':
      return (
        <>
          <rect x="10" y="4.5" width="12" height="23" rx="6" />
          <path d="M16 4.5 V13" />
          <Fill><rect x="12" y="7" width="2.6" height="4" rx="1" /></Fill>
          <Fill opacity="0.55"><rect x="17.4" y="7" width="2.6" height="4" rx="1" /></Fill>
          <path d="M5.5 21.5 C6.5 27 11 29 16 29 C21 29 25.5 27 26.5 21.5" opacity="0.5" />
          <path d="M6.5 11.5 L9 13.5 M25.5 11.5 L23 13.5" />
        </>
      );
    case 'hud-keystrokes':
      return (
        <>
          <KeyCap x={12} y={4} label="W" />
          <KeyCap x={4} y={13} label="A" />
          <KeyCap x={12} y={13} label="S" />
          <KeyCap x={20} y={13} label="D" />
          <rect x="7" y="22" width="18" height="5.5" rx="1.6" opacity="0.8" />
          <path d="M11 24.7 H21" opacity="0.65" />
        </>
      );
    case 'hud-clock':
      return (
        <>
          <circle cx="16" cy="16" r="10.5" />
          <circle cx="16" cy="16" r="1.7" />
          <path d="M16 9 V16 L21 19" />
          <path d="M16 3.5 V6 M16 26 V28.5 M3.5 16 H6 M26 16 H28.5" opacity="0.6" />
        </>
      );
    case 'hud-ping':
      return (
        <>
          <path d="M6 25 V19 M12 25 V15 M18 25 V11 M24 25 V6" />
          <Fill opacity="0.85"><rect x="22.6" y="6" width="2.8" height="19" rx="1" /></Fill>
          <path d="M4 27 H28" opacity="0.55" />
        </>
      );
    case 'hud-memory':
      return (
        <>
          <rect x="4" y="9" width="24" height="13" rx="2.2" />
          <path d="M8 22 V25 M13 22 V25 M19 22 V25 M24 22 V25" />
          <rect x="7.5" y="12" width="4.5" height="6" rx="0.8" />
          <rect x="13.7" y="12" width="4.5" height="6" rx="0.8" />
          <Fill opacity="0.85"><rect x="20" y="12" width="4.5" height="6" rx="0.8" /></Fill>
        </>
      );
    case 'hud-direction':
      return (
        <>
          <circle cx="16" cy="16" r="11" />
          <path d="M16 5 L19.5 16 L16 27 L12.5 16 Z" opacity="0.55" />
          <path d="M16 7 L18.5 16 L16 14 L13.5 16 Z" fill={C} stroke="none" />
          <path d="M16 25 L13.5 16 L16 18 L18.5 16 Z" opacity="0.8" />
          <circle cx="16" cy="16" r="1.4" fill={C} stroke="none" />
        </>
      );

    // ---------- VISUAL ----------
    case 'visual-crosshair':
      return (
        <>
          <path d="M16 3.5 V11 M16 21 V28.5 M3.5 16 H11 M21 16 H28.5" />
          <circle cx="16" cy="16" r="3.5" />
          <Fill><circle cx="16" cy="16" r="1.2" /></Fill>
          <path d="M5 5 H8.5 M5 5 V8.5 M27 5 H23.5 M27 5 V8.5 M5 27 H8.5 M5 27 V23.5 M27 27 H23.5 M27 27 V23.5" opacity="0.45" />
        </>
      );
    case 'visual-motionblur':
      return (
        <>
          <path d="M4 10 H15 M3 16 H12 M5 22 H14" opacity="0.75" />
          <path d="M14 8 L24 16 L14 24 Z" />
          <path d="M20 8 L28 16 L20 24" opacity="0.55" />
        </>
      );
    case 'visual-fog':
      return (
        <>
          <circle cx="22" cy="9" r="3.4" />
          <path d="M4 17 C8 14.5 11 19.5 15 17 S22 14.5 28 17" />
          <path d="M3 22 C7 19.5 10 24.5 14 22 S21 19.5 29 22" />
          <path d="M5 27 C9 24.5 12 29 16 27 S23 24.5 27 27" opacity="0.6" />
        </>
      );
    case 'visual-colors':
      return (
        <>
          <path d="M16 4 C9.4 4 4 8.8 4 14.8 C4 20.8 8.4 23.5 12 23.5 C14.3 23.5 15 22 15 20.4 C15 18.8 14 18 14 16.6 C14 15.2 15.2 14.5 16.6 14.5 H21.5 C25.5 14.5 28 12.2 28 8.8 C28 6 23.7 4 16 4 Z" />
          <Fill><circle cx="10.5" cy="11.5" r="1.8" /></Fill>
          <Fill opacity="0.8"><circle cx="16.5" cy="9.5" r="1.8" /></Fill>
          <Fill opacity="0.6"><circle cx="22" cy="11" r="1.8" /></Fill>
        </>
      );
    case 'visual-zoom':
      return (
        <>
          <circle cx="14" cy="14" r="8" />
          <path d="M20 20 L27 27" />
          <path d="M10.5 11 C11.6 9.6 13.2 8.8 15 8.8" opacity="0.7" />
          <Fill opacity="0.7"><circle cx="14" cy="14" r="2.2" /></Fill>
        </>
      );

    // ---------- GAMEPLAY ----------
    case 'game-drift':
      return (
        <>
          <path d="M6 6 L19 19 M26 6 L13 19" />
          <path d="M4.5 4.5 L9 9 L6 12 L3.5 9.5 Z M27.5 4.5 L23 9 L26 12 L28.5 9.5 Z" fill={C} stroke="none" />
          <path d="M11 22 L10 27 L16 24 L22 27 L21 22" />
          <path d="M9 27 H23" opacity="0.6" />
        </>
      );
    case 'game-tps':
      return (
        <>
          <circle cx="16" cy="16" r="9.5" />
          <circle cx="16" cy="16" r="4.5" />
          <path d="M16 2.5 V6.5 M16 25.5 V29.5 M2.5 16 H6.5 M25.5 16 H29.5" />
          <Fill><circle cx="16" cy="16" r="1.5" /></Fill>
        </>
      );
    case 'game-haste':
      return (
        <>
          <path d="M7 8 C13 4 21 4 27 9 L20 15 C16 12 12 11 7 8 Z" />
          <path d="M20 15 L27 9 L25 18 L20 15 Z" fill={C} stroke="none" />
          <path d="M17 16 L8 27" />
          <path d="M6 28 L9 25" opacity="0.65" />
        </>
      );

    // ---------- UTILITIES ----------
    case 'util-keybinds':
      return (
        <>
          <rect x="3.5" y="8" width="25" height="16" rx="2.5" />
          <path d="M7 12 H10 M13 12 H16 M19 12 H25 M7 16 H11 M14 16 H18 M21 16 H25 M9 20 H23" />
          <rect x="17" y="4" width="10" height="7" rx="1.5" fill="#101725" />
          <text x="22" y="9.2" fontSize="5" fontFamily="monospace" fontWeight="700" textAnchor="middle" stroke="none" fill={C}>F8</text>
        </>
      );
    case 'util-session':
      return (
        <>
          <circle cx="16" cy="17.5" r="9.5" />
          <path d="M13 4.5 H19 M16 4.5 V8 M22 9 L24 7" />
          <path d="M16 12 V17.5 L20 20" />
          <Fill><circle cx="16" cy="17.5" r="1.4" /></Fill>
        </>
      );
    case 'util-screens':
      return (
        <>
          <rect x="3.5" y="6" width="25" height="16" rx="2.4" />
          <path d="M11 26 H21 M16 22 V26" />
          <circle cx="16" cy="14" r="4.5" />
          <Fill><circle cx="16" cy="14" r="1.6" /></Fill>
        </>
      );

    // ---------- COSMÉTICOS ----------
    default: {
      const lower = id.toLowerCase();
      if (lower.includes('wing')) {
        return (
          <>
            <path d="M16 15 C11 7 5 6 3 8 C2 14 6 21 14 24 L16 15 Z" />
            <path d="M16 15 C21 7 27 6 29 8 C30 14 26 21 18 24 L16 15 Z" />
            <path d="M5.5 9.5 C8 12 10 15 12 19 M26.5 9.5 C24 12 22 15 20 19" opacity="0.65" />
            <Fill><path d="M15 13 L16 10 L17 13 L20 14 L17 15 L16 18 L15 15 L12 14 Z" /></Fill>
          </>
        );
      }
      if (lower.includes('cape')) {
        return (
          <>
            <path d="M8 5 L16 9 L24 5 L27 26 L16 29 L5 26 Z" />
            <path d="M8 5 L16 9 L24 5" opacity="0.65" />
            <path d="M12 12 V23 M20 12 V23" opacity="0.5" />
            <Fill><path d="M14 15 L16 12 L18 15 L16 18 Z" /></Fill>
          </>
        );
      }
      return (
        <>
          <path d="M16 4 L27 10 L16 16 L5 10 Z" />
          <path d="M5 10 L16 16 L16 28 L5 22 Z" opacity="0.7" />
          <path d="M27 10 L16 16 L16 28 L27 22 Z" opacity="0.45" />
          <path d="M10 13 V21 M22 13 V21" opacity="0.5" />
        </>
      );
    }
  }
}

export function ModuleArt({ id, className = '' }: { id: string; className?: string }) {
  return (
    <svg
      viewBox="0 0 32 32"
      className={`art-svg module-icon ${className}`}
      fill="none"
      stroke={C}
      strokeWidth={1.8}
      strokeLinecap="round"
      strokeLinejoin="round"
      shapeRendering="geometricPrecision"
      aria-hidden="true"
      focusable="false"
    >
      <rect x="1.5" y="1.5" width="29" height="29" rx="7" fill={C} stroke="none" opacity="0.08" />
      {glyphFor(id)}
    </svg>
  );
}

/** Alias semántico para los lugares donde se usa como icono y no como ilustración. */
export const ModuleIcon = ModuleArt;
