// @lex/ui-components — Componentes base de LEX CLIENT.
// Familia visual: superficies profundas, acento cian, tipografía JetBrains Mono para datos.

import type React from 'react';

export type { ComponentProps } from 'react';

export function Button(props: {
  variant?: 'primary' | 'secondary' | 'ghost' | 'danger';
  size?: 'sm' | 'md' | 'lg';
  children: React.ReactNode;
  iconLeft?: React.ReactNode;
  iconRight?: React.ReactNode;
  disabled?: boolean;
} & React.ButtonHTMLAttributes<HTMLButtonElement>) {
  const {
    variant = 'secondary',
    size = 'md',
    iconLeft,
    iconRight,
    className = '',
    children,
    ...rest
  } = props;
  return (
    <button className={`lex-btn lex-btn-${variant} lex-btn-${size} ${className}`} {...rest}>
      {iconLeft && <span className="lex-btn-icon">{iconLeft}</span>}
      <span className="lex-btn-label">{children}</span>
      {iconRight && <span className="lex-btn-icon">{iconRight}</span>}
    </button>
  );
}

export function Card(props: {
  children: React.ReactNode;
  title?: string;
  subtitle?: string;
  className?: string;
  action?: React.ReactNode;
}) {
  const { children, title, subtitle, action, className = '' } = props;
  return (
    <section className={`lex-card ${className}`}>
      {(title || action) && (
        <header className="lex-card-head">
          <div className="lex-card-heading">
            {title && <h3 className="lex-card-title">{title}</h3>}
            {subtitle && <p className="lex-card-subtitle">{subtitle}</p>}
          </div>
          {action && <div className="lex-card-action">{action}</div>}
        </header>
      )}
      <div className="lex-card-content">{children}</div>
    </section>
  );
}

export function Badge(props: {
  children: React.ReactNode;
  variant?: 'default' | 'success' | 'warning' | 'error' | 'accent';
}) {
  const { children, variant = 'default' } = props;
  return <span className={`lex-badge lex-badge-${variant}`}>{children}</span>;
}

export function Toggle(props: {
  on: boolean;
  onChange: (v: boolean) => void;
  disabled?: boolean;
  label?: React.ReactNode;
}) {
  const { on, onChange, disabled, label } = props;
  return (
    <span className={`lex-toggle-row ${disabled ? 'is-disabled' : ''}`}>
      {label && <span className="lex-toggle-label">{label}</span>}
      <button
        type="button"
        role="switch"
        aria-checked={on}
        aria-label={typeof label === 'string' ? label : undefined}
        disabled={disabled}
        onClick={() => !disabled && onChange(!on)}
        className={`lex-toggle ${on ? 'is-on' : ''}`}
      >
        <span className="lex-toggle-knob" />
      </button>
    </span>
  );
}

export function Skeleton(props: { className?: string; w?: string; h?: string }) {
  return <span className={`lex-skeleton ${props.className ?? ''} ${props.w ? `w-${props.w}` : ''} ${props.h ? `h-${props.h}` : ''}`} />;
}

export function Brand(props: { size?: 'sm' | 'md' | 'lg' }) {
  const { size = 'md' } = props;
  return (
    <div className={`lex-brand lex-brand-${size}`}>
      <span className="lex-brand-mark" aria-hidden="true">
        <span className="lex-brand-mark-core">L</span>
        <span className="lex-brand-mark-scan" />
      </span>
      <span className="lex-brand-copy">
        <span className="lex-brand-name">LEX<span>CLIENT</span></span>
        <span className="lex-brand-sub">MINECRAFT · WINDOWS</span>
      </span>
    </div>
  );
}
