// LEX CLIENT — Ilustraciones originales para el Mod Menu.
// Lenguaje visual propio: pixel-art temático de Minecraft, paleta limitada y rejilla de 64x64.
// No se copian recursos de Lunar / Badlion / LabyMod / Flarial: sólo se estudia el concepto
// (tarjetas ilustradas) y se dibuja una familia gráfica coherente para LEX.

import React from 'react';

type Box = [number, number, number, number];

const INK = '#0c0f18';
const DEEP = '#1b2231';
const SLATE = '#39445b';
const STEEL = '#64718b';
const SILVER = '#9eabc2';
const WHITE = '#eef3ff';
const GOLD = '#ffcc5c';
const GREEN = '#5bd68e';
const RED = '#ff6274';
const CREAM = '#efe6cd';

/** Conjunto de rectángulos pixelados con un mismo color. */
function R({ color, boxes, opacity }: { color: string; boxes: Box[]; opacity?: number }) {
  return (
    <>
      {boxes.map(([x, y, w, h], i) => (
        <rect key={i} x={x} y={y} width={w} height={h} fill={color} opacity={opacity} />
      ))}
    </>
  );
}

function Accent({ boxes, opacity }: { boxes: Box[]; opacity?: number }) {
  return <R color="currentColor" boxes={boxes} opacity={opacity} />;
}

function Key({ x, y, label, hot = false }: { x: number; y: number; label: string; hot?: boolean }) {
  return (
    <g>
      <R color={INK} boxes={[[x, y, 18, 18]]} />
      <R color={hot ? STEEL : SLATE} boxes={[[x + 2, y + 2, 14, 14]]} />
      <R color={WHITE} boxes={[[x + 3, y + 3, 12, 2]]} opacity={0.32} />
      <text x={x + 9} y={y + 13} fontSize="9" fontFamily="monospace" fontWeight="700" fill={WHITE} textAnchor="middle">
        {label}
      </text>
    </g>
  );
}

function artFor(id: string): React.ReactNode {
  switch (id) {
    // ===== HUD =====
    case 'hud-fps':
      return (
        <>
          <R color={INK} boxes={[[6, 10, 52, 38], [28, 48, 8, 6], [18, 54, 28, 5]]} />
          <R color={SLATE} boxes={[[8, 12, 48, 34], [10, 14, 44, 30], [30, 48, 4, 6]]} />
          <R color={DEEP} boxes={[[12, 16, 40, 26]]} />
          <R color={GREEN} boxes={[[16, 34, 6, 8], [26, 28, 6, 14], [36, 24, 6, 18], [46, 30, 4, 12]]} />
          <R color={WHITE} boxes={[[16, 32, 6, 2], [26, 26, 6, 2], [36, 22, 6, 2]]} opacity={0.85} />
          <R color={STEEL} boxes={[[20, 55, 24, 2]]} />
        </>
      );

    case 'hud-cps':
      return (
        <>
          <R color={INK} boxes={[[21, 8, 22, 42], [19, 20, 4, 22], [41, 20, 4, 22]]} />
          <R color={SLATE} boxes={[[23, 10, 18, 38], [21, 22, 2, 18], [41, 22, 2, 18]]} />
          <R color={STEEL} boxes={[[23, 12, 8, 18], [33, 12, 8, 18]]} />
          <Accent boxes={[[24, 12, 7, 14]]} opacity={0.9} />
          <R color={INK} boxes={[[31, 12, 2, 16]]} />
          <R color={WHITE} boxes={[[30, 20, 4, 8]]} />
          <R color={GOLD} boxes={[[14, 12, 4, 4], [46, 12, 4, 4], [12, 18, 2, 2], [50, 18, 2, 2]]} />
        </>
      );

    case 'hud-keystrokes':
      return (
        <>
          <Key x={22} y={6} label="W" hot />
          <Key x={6} y={28} label="A" />
          <Key x={22} y={28} label="S" />
          <Key x={38} y={28} label="D" />
        </>
      );

    case 'hud-clock':
      return (
        <>
          <circle cx="32" cy="32" r="23" fill={INK} />
          <circle cx="32" cy="32" r="20" fill={SLATE} />
          <circle cx="32" cy="32" r="16" fill={CREAM} />
          <R color={INK} boxes={[[31, 13, 2, 5], [31, 46, 2, 5], [13, 31, 5, 2], [46, 31, 5, 2]]} />
          <R color={INK} boxes={[[30, 17, 4, 16], [32, 30, 16, 4]]} />
          <circle cx="32" cy="32" r="3" fill={GOLD} />
        </>
      );

    case 'hud-ping':
      return (
        <>
          <rect x="12" y="34" width="8" height="10" fill={RED} />
          <rect x="23" y="26" width="8" height="18" fill={GOLD} />
          <rect x="34" y="18" width="8" height="26" fill="currentColor" />
          <rect x="45" y="10" width="8" height="34" fill={GREEN} />
          <R color={WHITE} boxes={[[12, 34, 8, 2], [23, 26, 8, 2], [34, 18, 8, 2], [45, 10, 8, 2]]} opacity={0.55} />
          <R color={INK} boxes={[[8, 46, 48, 7]]} />
          <R color={STEEL} boxes={[[10, 46, 44, 2]]} />
        </>
      );

    case 'hud-memory':
      return (
        <>
          <R color={INK} boxes={[[6, 18, 52, 30]]} />
          <R color="#1f6b52" boxes={[[8, 20, 48, 26]]} />
          <R color={INK} boxes={[[28, 20, 8, 5]]} />
          {[0, 1, 2, 3].map((i) => (
            <g key={i}>
              <R color={INK} boxes={[[12 + i * 11, 25, 9, 15]]} />
              <R color={SLATE} boxes={[[14 + i * 11, 27, 5, 11]]} />
            </g>
          ))}
          <R color={GOLD} boxes={[[10, 46, 7, 3], [21, 46, 7, 3], [36, 46, 7, 3], [47, 46, 7, 3]]} />
        </>
      );

    case 'hud-direction':
      return (
        <>
          <circle cx="32" cy="32" r="23" fill={INK} />
          <circle cx="32" cy="32" r="20" fill={SLATE} />
          <circle cx="32" cy="32" r="16" fill={CREAM} />
          <text x="32" y="21" fontSize="8" fontFamily="monospace" fontWeight="700" fill={INK} textAnchor="middle">N</text>
          <text x="32" y="51" fontSize="8" fontFamily="monospace" fontWeight="700" fill={INK} textAnchor="middle">S</text>
          <text x="16" y="35" fontSize="8" fontFamily="monospace" fontWeight="700" fill={INK} textAnchor="middle">W</text>
          <text x="48" y="35" fontSize="8" fontFamily="monospace" fontWeight="700" fill={INK} textAnchor="middle">E</text>
          <path d="M32 21 L37 32 L32 30 L27 32 Z" fill={RED} />
          <path d="M32 43 L27 32 L32 34 L37 32 Z" fill={STEEL} />
          <circle cx="32" cy="32" r="2.5" fill={GOLD} />
        </>
      );

    // ===== VISUAL =====
    case 'visual-crosshair':
      return (
        <>
          <R color={SLATE} boxes={[[6, 6, 6, 6], [52, 6, 6, 6], [6, 52, 6, 6], [52, 52, 6, 6]]} opacity={0.45} />
          <R color={INK} boxes={[[29, 8, 6, 18], [29, 38, 6, 18], [8, 29, 18, 6], [38, 29, 18, 6]]} />
          <R color={WHITE} boxes={[[30, 10, 4, 14], [30, 40, 4, 14], [10, 30, 14, 4], [40, 30, 14, 4]]} />
          <Accent boxes={[[31, 31, 2, 2]]} />
        </>
      );

    case 'visual-motionblur':
      return (
        <>
          <Accent boxes={[[6, 15, 20, 3], [4, 23, 26, 3], [8, 31, 17, 3], [6, 39, 21, 3]]} opacity={0.55} />
          <R color={WHITE} boxes={[[10, 19, 11, 2], [12, 35, 9, 2]]} opacity={0.5} />
          <path d="M32 14 L50 24 L32 34 L14 24 Z" fill={SILVER} stroke={INK} strokeWidth="2" />
          <path d="M14 24 L32 34 L32 52 L14 42 Z" fill={SLATE} stroke={INK} strokeWidth="2" />
          <path d="M50 24 L32 34 L32 52 L50 42 Z" fill={STEEL} stroke={INK} strokeWidth="2" />
        </>
      );

    case 'visual-fog':
      return (
        <>
          <R color={GOLD} boxes={[[44, 9, 10, 10]]} />
          <path d="M2 50 L20 27 L34 46 L46 30 L62 50 Z" fill={SLATE} />
          <path d="M0 54 L16 38 L32 54 Z" fill={STEEL} />
          <R color={WHITE} boxes={[[0, 34, 64, 5], [0, 41, 64, 6], [0, 48, 64, 7]]} opacity={0.16} />
          <R color={WHITE} boxes={[[6, 35, 24, 2], [34, 42, 22, 2], [10, 49, 30, 2]]} opacity={0.32} />
        </>
      );

    case 'visual-colors':
      return (
        <>
          <path
            d="M14 12 h36 v14 c0 8-7 10-12 8-4-2-8-2-11 2-3 4-2 10-9 10C10 46 7 38 7 30V18c0-4 3-6 7-6z"
            fill={INK}
          />
          <path
            d="M16 14 h32 v12 c0 7-6 9-11 7-4-2-8-2-11 2-3 4-2 9-8 9-5 0-7-6-7-13V18c0-3 2-4 5-4z"
            fill="#a97745"
          />
          <circle cx="20" cy="26" r="4" fill={RED} />
          <circle cx="32" cy="23" r="4" fill={GOLD} />
          <circle cx="44" cy="26" r="4" fill={GREEN} />
          <circle cx="27" cy="36" r="4" fill="currentColor" />
          <path d="M48 38 c3 5 4 7 4 9a4 4 0 0 1-8 0c0-2 1-4 4-9z" fill="currentColor" />
        </>
      );

    case 'visual-zoom':
      return (
        <>
          <path d="M38 38 L55 55 L49 61 L32 44 Z" fill={INK} />
          <path d="M40 40 L51 51 L48 54 L37 43 Z" fill={STEEL} />
          <circle cx="26" cy="26" r="17" fill={INK} />
          <circle cx="26" cy="26" r="14" fill={STEEL} />
          <circle cx="26" cy="26" r="11" fill="currentColor" opacity={0.45} />
          <circle cx="26" cy="26" r="9" fill="#bfe9ff" opacity={0.25} />
          <path d="M18 20 q6-7 13-5" stroke={WHITE} strokeWidth="3" fill="none" opacity={0.8} />
          <R color={GOLD} boxes={[[8, 8, 3, 3], [46, 12, 2, 2], [12, 46, 2, 2]]} />
        </>
      );

    // ===== GAMEPLAY / PVP =====
    case 'game-drift':
      return (
        <>
          <R color={INK} boxes={[[16, 10, 32, 35]]} />
          <R color="#e7e2d2" boxes={[[18, 12, 28, 30]]} />
          <R color={INK} boxes={[[22, 21, 8, 9], [34, 21, 8, 9]]} />
          <R color={RED} boxes={[[24, 23, 4, 5], [36, 23, 4, 5]]} />
          <R color={INK} boxes={[[30, 30, 4, 5], [20, 40, 24, 6]]} />
          <R color="#e7e2d2" boxes={[[22, 42, 4, 4], [30, 42, 4, 4], [38, 42, 4, 4]]} />
          <R color={STEEL} boxes={[[26, 14, 2, 5]]} opacity={0.7} />
        </>
      );

    case 'game-tps':
      return (
        <>
          <R color={INK} boxes={[[28, 5, 8, 9], [28, 50, 8, 9], [5, 28, 9, 8], [50, 28, 9, 8], [12, 12, 9, 9], [43, 12, 9, 9], [12, 43, 9, 9], [43, 43, 9, 9]]} />
          <circle cx="32" cy="32" r="20" fill={INK} />
          <circle cx="32" cy="32" r="16" fill={STEEL} />
          <circle cx="32" cy="32" r="12" fill={DEEP} />
          <text x="32" y="38" fontSize="15" fontFamily="monospace" fontWeight="700" fill="currentColor" textAnchor="middle">20</text>
        </>
      );

    case 'game-haste':
      return (
        <>
          <path d="M20 13 h13 v20 l16 8 v10 H18 v-8 l4-6 z" fill={INK} />
          <path d="M23 16 h8 v19 l14 7 v5 H21 v-7 l4-5 z" fill={STEEL} />
          <R color={INK} boxes={[[16, 47, 35, 7]]} />
          <R color={WHITE} boxes={[[18, 49, 31, 2]]} />
          <path d="M18 18 L6 13 L10 23 L4 26 L16 31 Z" fill={SILVER} />
          <path d="M35 18 L47 13 L43 23 L49 26 L37 31 Z" fill={SILVER} />
          <Accent boxes={[[7, 38, 11, 2], [6, 44, 14, 2]]} opacity={0.75} />
        </>
      );

    // ===== UTILITIES =====
    case 'util-keybinds':
      return (
        <>
          <R color={INK} boxes={[[8, 18, 40, 30]]} />
          <R color={SLATE} boxes={[[10, 20, 36, 26]]} />
          <R color={STEEL} boxes={[[12, 22, 32, 3]]} />
          <text x="28" y="38" fontSize="13" fontFamily="monospace" fontWeight="700" fill={WHITE} textAnchor="middle">KEY</text>
          <R color={INK} boxes={[[40, 7, 16, 14]]} />
          <R color={GOLD} boxes={[[42, 9, 12, 10]]} />
          <text x="48" y="17" fontSize="8" fontFamily="monospace" fontWeight="700" fill={INK} textAnchor="middle">R</text>
        </>
      );

    case 'util-session':
      return (
        <>
          <R color={INK} boxes={[[17, 8, 30, 5], [17, 51, 30, 5]]} />
          <R color={STEEL} boxes={[[20, 10, 4, 44], [40, 10, 4, 44]]} />
          <path d="M24 15 L40 15 L33 30 L31 30 Z" fill={SILVER} opacity={0.8} />
          <path d="M31 34 L33 34 L40 49 L24 49 Z" fill={SILVER} opacity={0.8} />
          <path d="M27 19 L37 19 L33 27 L31 27 Z" fill={GOLD} />
          <R color={GOLD} boxes={[[28, 44, 8, 4], [31, 32, 2, 4]]} />
        </>
      );

    case 'util-screens':
      return (
        <>
          <R color={INK} boxes={[[6, 18, 52, 34], [16, 12, 20, 8]]} />
          <R color={SLATE} boxes={[[8, 20, 48, 30]]} />
          <R color={GOLD} boxes={[[38, 14, 6, 4]]} />
          <circle cx="32" cy="35" r="13" fill={INK} />
          <circle cx="32" cy="35" r="11" fill={STEEL} />
          <circle cx="32" cy="35" r="8" fill="currentColor" opacity={0.6} />
          <circle cx="32" cy="35" r="5" fill={DEEP} />
          <circle cx="29" cy="32" r="2" fill={WHITE} opacity={0.9} />
          <R color={GOLD} boxes={[[12, 24, 6, 4]]} />
        </>
      );

    default:
      // Ilustración genérica de "bloque" para ids no catalogados.
      return (
        <>
          <path d="M32 10 L54 22 L32 34 L10 22 Z" fill={SILVER} stroke={INK} strokeWidth="2" />
          <path d="M10 22 L32 34 L32 54 L10 42 Z" fill={SLATE} stroke={INK} strokeWidth="2" />
          <path d="M54 22 L32 34 L32 54 L54 42 Z" fill={STEEL} stroke={INK} strokeWidth="2" />
        </>
      );
  }
}

export function ModuleArt({ id, className = '' }: { id: string; className?: string }) {
  return (
    <svg viewBox="0 0 64 64" className={`art-svg ${className}`} shapeRendering="crispEdges" aria-hidden="true" focusable="false">
      {artFor(id)}
    </svg>
  );
}
