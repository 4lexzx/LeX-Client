// LEX CLIENT — Iconografía de control del Mod Menu (dibujada a mano, 16x16).
// No se usan librerías de iconos externas: misma familia que las ilustraciones de módulos.

import React from 'react';

export type IconName =
  | 'search' | 'star' | 'grid' | 'list' | 'gear' | 'close' | 'filter'
  | 'chevron' | 'spark' | 'copy' | 'check' | 'info' | 'home' | 'box'
  | 'all' | 'hud' | 'pvp' | 'visual' | 'performance' | 'utility' | 'cosmetics'
  | 'play' | 'shield' | 'pulse' | 'move' | 'keyboard' | 'layers' | 'clock' | 'target' | 'arrow' | 'wrench' | 'folder';

const stroke = { fill: 'none', stroke: 'currentColor', strokeWidth: 1.5, strokeLinecap: 'square' as const, strokeLinejoin: 'miter' as const };

function shape(name: IconName): React.ReactNode {
  switch (name) {
    case 'search':
      return <><circle cx="7" cy="7" r="4.4" {...stroke} /><path d="M10.4 10.4 L14 14" {...stroke} /></>;
    case 'star':
      return <path d="M8 1.6 l1.9 4 4.4.6-3.2 3.1.8 4.4L8 11.6l-3.9 2.1.8-4.4L1.7 6.2l4.4-.6z" fill="currentColor" />;
    case 'grid':
      return <><rect x="2" y="2" width="5" height="5" rx="0.6" fill="currentColor" /><rect x="9" y="2" width="5" height="5" rx="0.6" fill="currentColor" /><rect x="2" y="9" width="5" height="5" rx="0.6" fill="currentColor" /><rect x="9" y="9" width="5" height="5" rx="0.6" fill="currentColor" /></>;
    case 'list':
      return <><rect x="2" y="3" width="12" height="2" fill="currentColor" /><rect x="2" y="7" width="12" height="2" fill="currentColor" /><rect x="2" y="11" width="12" height="2" fill="currentColor" /></>;
    case 'gear':
      return (
        <>
          <path d="M7 1.6h2l.4 2 1.4.6 1.7-1.1 1.4 1.4-1.1 1.7.6 1.4 2 .4v2l-2 .4-.6 1.4 1.1 1.7-1.4 1.4-1.7-1.1-1.4.6-.4 2H7l-.4-2-1.4-.6-1.7 1.1-1.4-1.4 1.1-1.7-.6-1.4-2-.4v-2l2-.4.6-1.4L2.5 5.5 3.9 4.1l1.7 1.1 1.4-.6z" {...stroke} />
          <circle cx="8" cy="8" r="2.2" {...stroke} />
        </>
      );
    case 'close':
      return <><path d="M3 3 L13 13" {...stroke} /><path d="M13 3 L3 13" {...stroke} /></>;
    case 'filter':
      return <path d="M2 3h12l-4.6 5.4V13L6.6 11V8.4z" {...stroke} />;
    case 'chevron':
      return <path d="M4 6 L8 10 L12 6" {...stroke} />;
    case 'spark':
      return <><path d="M8 1.5 l1.3 4.2 4.2 1.3-4.2 1.3L8 12.5 6.7 8.3 2.5 7l4.2-1.3z" fill="currentColor" /><path d="M13 11.5 l.6 1.6 1.6.6-1.6.6-.6 1.6-.6-1.6-1.6-.6 1.6-.6z" fill="currentColor" opacity="0.7" /></>;
    case 'copy':
      return <><rect x="5" y="5" width="9" height="10" rx="1" {...stroke} /><path d="M11 5V3H3v9h2" {...stroke} /></>;
    case 'check':
      return <path d="M2.5 8.5 L6 12 L13.5 4" {...stroke} />;
    case 'info':
      return <><circle cx="8" cy="8" r="6.2" {...stroke} /><path d="M8 7v5" {...stroke} /><rect x="7.2" y="4.5" width="1.6" height="1.6" fill="currentColor" /></>;
    case 'home':
      return <><path d="M2 7.5 L8 2.5 L14 7.5 V14 H2 Z" {...stroke} /><path d="M6.5 14 V9.5 H9.5 V14" {...stroke} /></>;
    case 'box':
      return <><path d="M8 1.8 L14 5 V11 L8 14.2 L2 11 V5 Z" {...stroke} /><path d="M2 5 L8 8.2 14 5 M8 8.2 V14.2" {...stroke} /></>;
    case 'all':
      return <><rect x="2" y="2" width="5" height="5" fill="currentColor" opacity="0.9" /><rect x="9" y="2" width="5" height="5" fill="currentColor" opacity="0.6" /><rect x="2" y="9" width="5" height="5" fill="currentColor" opacity="0.6" /><rect x="9" y="9" width="5" height="5" fill="currentColor" opacity="0.9" /></>;
    case 'hud':
      return <><rect x="1.5" y="2.5" width="13" height="9" rx="1" {...stroke} /><path d="M6 14h4M8 11.5V14" {...stroke} /></>;
    case 'pvp':
      return <><path d="M2.5 13.5 L11 5 M9.5 2.5 L13.5 6.5 L11.5 8.5 L7.5 4.5 z" {...stroke} /><path d="M13.5 13.5 L5 5 M6.5 2.5 L2.5 6.5 L4.5 8.5 L8.5 4.5 z" {...stroke} opacity="0.6" /></>;
    case 'visual':
      return <><path d="M1.5 8 S4 4 8 4 14.5 8 14.5 8 12 12 8 12 1.5 8 1.5 8z" {...stroke} /><circle cx="8" cy="8" r="2" fill="currentColor" /></>;
    case 'performance':
      return <path d="M9 1.5 L3.5 9 H7.5 L6.5 14.5 L12.5 7 H8.5 z" fill="currentColor" />;
    case 'utility':
      return <path d="M13.8 2.2a4.5 4.5 0 0 0-5.7 5.8L3 13.1 4.9 15l5.1-5.1a4.5 4.5 0 0 0 5.8-5.7l-2.6 2.6-2.3-.5-.5-2.3z" {...stroke} />;
    case 'cosmetics':
      return <><path d="M8 1.5 L14 6 L11.7 13 H4.3 L2 6 z" {...stroke} /><path d="M2 6h12M5.5 6 L8 13 10.5 6" {...stroke} opacity="0.7" /></>;
    case 'play':
      return <><rect x="1.8" y="1.8" width="12.4" height="12.4" rx="3" {...stroke} /><path d="M6.4 5.2 L11 8 L6.4 10.8 Z" fill="currentColor" /></>;
    case 'shield':
      return <><path d="M8 1.8 L13.2 4 V8.2 C13.2 11.3 11 13.6 8 14.3 C5 13.6 2.8 11.3 2.8 8.2 V4 Z" {...stroke} /><path d="M5.5 8 L7.2 9.8 L10.8 6.2" {...stroke} /></>;
    case 'pulse':
      return <><path d="M1.5 8 H4.5 L6.5 3.5 L9 12.5 L11 7 L14.5 7" {...stroke} /></>;
    case 'move':
      return <><path d="M8 1.8 V14.2 M1.8 8 H14.2 M8 1.8 L6 4 M8 1.8 L10 4 M8 14.2 L6 12 M8 14.2 L10 12 M1.8 8 L4 6 M1.8 8 L4 10 M14.2 8 L12 6 M14.2 8 L12 10" {...stroke} /></>;
    case 'keyboard':
      return <><rect x="1.5" y="4" width="13" height="8" rx="1.5" {...stroke} /><path d="M4 6.5h1M7 6.5h1M10 6.5h1M4 9h7" {...stroke} /></>;
    case 'layers':
      return <><path d="M8 1.8 L14.2 5 L8 8.2 L1.8 5 Z" {...stroke} /><path d="M1.8 8.5 L8 11.7 L14.2 8.5 M1.8 12 L8 15.2 L14.2 12" {...stroke} opacity="0.65" /></>;
    case 'clock':
      return <><circle cx="8" cy="8" r="6.2" {...stroke} /><path d="M8 4.6 V8 L10.4 9.5" {...stroke} /></>;
    case 'target':
      return <><circle cx="8" cy="8" r="6" {...stroke} /><circle cx="8" cy="8" r="2.4" {...stroke} /><path d="M8 0.8V3 M8 13V15.2 M0.8 8H3 M13 8H15.2" {...stroke} opacity="0.65" /></>;
    case 'arrow':
      return <><path d="M3 8 H12.5 M9 4.5 L12.5 8 L9 11.5" {...stroke} /></>;
    case 'wrench':
      return <path d="M13.4 2.6a3.7 3.7 0 0 0-4.8 4.7L3 12.9 4.7 14.6l5.6-5.6a3.7 3.7 0 0 0 4.7-4.8l-2.4 2.4-2.1-.4-.4-2.1z" {...stroke} />;
    case 'folder':
      return <><path d="M1.8 4.2h4.4l1.4 1.8h6.6v7.8H1.8z" {...stroke} /><path d="M1.8 7.8h12.4" {...stroke} opacity="0.6" /></>;
    default:
      return null;
  }
}

export function UiIcon({ name, size = 16, className = '' }: { name: IconName; size?: number; className?: string }) {
  return (
    <svg viewBox="0 0 16 16" width={size} height={size} className={className} aria-hidden="true" focusable="false">
      {shape(name)}
    </svg>
  );
}
