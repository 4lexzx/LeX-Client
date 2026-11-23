// @lex/ui-components — Componentes de UI reutilizables para LEX CLIENT (React).
// Estilo propio: paleta oscura #101116, azul eléctrico, fuentes modernas (Inter).

export type { ComponentProps } from 'react';

export function Button(props: {
  variant?: 'primary' | 'secondary' | 'ghost' | 'danger';
  size?: 'sm' | 'md' | 'lg';
  children: React.ReactNode;
  iconLeft?: React.ReactNode;
  iconRight?: React.ReactNode;
  disabled?: boolean;
} & React.ButtonHTMLAttributes<HTMLButtonElement>) {
  const { variant = 'secondary', size = 'md', iconLeft, iconRight, ...rest } = props;
  const base =
    'inline-flex items-center justify-center gap-2 rounded-lg font-medium transition-all active:scale-[0.98]';
  const sizes: Record<string, string> = {
    sm: 'px-3 py-1.5 text-xs',
    md: 'px-4 py-2 text-sm',
    lg: 'px-6 py-3 text-sm font-semibold',
  };
  const variants: Record<string, string> = {
    primary: 'bg-[var(--accent)] text-white hover:brightness-110 disabled:opacity-40',
    secondary: 'bg-[var(--surface)] text-[var(--text)] hover:bg-[var(--surface-2)] disabled:opacity-40',
    ghost: 'bg-transparent text-[var(--text)] hover:bg-[var(--surface)]',
    danger: 'bg-[#E5484D]/10 text-[#E5484D] hover:bg-[#E5484D]/20',
  };
  return (
    <button className={`${base} ${sizes[size]} ${variants[variant]} ${rest.className ?? ''}`} {...rest}>
      {iconLeft}
      <span>{props.children}</span>
      {iconRight}
    </button>
  );
}

export function Card(props: { children: React.ReactNode; title?: string; subtitle?: string; className?: string; action?: React.ReactNode }) {
  const { children, title, subtitle, action, className } = props;
  return (
    <section className={`rounded-xl bg-[var(--surface)] p-4 border border-[var(--border)] ${className ?? ''}`}>
      {title && (
        <div className="flex items-center justify-between gap-4 mb-3">
          <div>
            <h3 className="text-sm font-semibold text-[var(--text)]">{title}</h3>
            {subtitle && <p className="text-xs text-[var(--text-secondary)] mt-0.5">{subtitle}</p>}
          </div>
          {action}
        </div>
      )}
      {children}
    </section>
  );
}

export function Badge(props: { children: React.ReactNode; variant?: 'default' | 'success' | 'warning' | 'error' | 'accent' }) {
  const { children, variant = 'default' } = props;
  const styles: Record<string, string> = {
    default: 'bg-[var(--surface-2)] text-[var(--text-secondary)] border-[var(--border)]',
    success: 'bg-[#34C286]/10 text-[#2EA36C] border-[#2EA36C]/30',
    warning: 'bg-[#D9A423]/10 text-[#B8863F] border-[#B8863F]/30',
    error: 'bg-[#E5484D]/10 text-[#C33C38] border-[#C33C38]/30',
    accent: 'bg-[var(--accent)]/10 text-[var(--accent)] border-[var(--accent)]/30',
  };
  return (
    <span className={`inline-flex items-center px-2 py-0.5 rounded-full border text-[10px] font-bold uppercase tracking-wider ${styles[variant]}`}>
      {children}
    </span>
  );
}

export function Toggle(props: { on: boolean; onChange: (v: boolean) => void; disabled?: boolean; label?: React.ReactNode; }) {
  const { on, onChange, disabled, label } = props;
  return (
    <label className="flex items-center justify-between cursor-pointer">
      {label && <span className="text-sm text-[var(--text)]">{label}</span>}
      <button
        role="switch"
        aria-checked={on}
        disabled={disabled}
        onClick={() => !disabled && onChange(!on)}
        className={`relative h-5 w-9 rounded-full transition-colors disabled:opacity-40 ${on ? 'bg-[var(--accent)]' : 'bg-[var(--surface-2)] border border-[var(--border)]'}`}
      >
        <span className={`absolute top-[2px] h-4 w-4 rounded-full transition-transform bg-white ${on ? 'translate-x-4' : 'translate-x-0.5'}`} />
      </button>
    </label>
  );
}

export function Skeleton(props: { className?: string; w?: string; h?: string }) {
  return <span className={`animate-pulse rounded bg-[var(--surface-2)] ${props.className ?? ''} ${props.w ? `w-${props.w}` : ''} ${props.h ? `h-${props.h}` : ''}`} />;
}

export function Brand(props: { size?: 'sm' | 'md' | 'lg' }) {
  const { size = 'md' } = props;
  const map = { sm: 'h-4', md: 'h-6', lg: 'h-8' };
  return (
    <div className={`${map[size]} flex items-center gap-2`}>
      <div className="w-8 h-8 rounded-lg bg-[var(--accent)] flex items-center justify-center text-white font-black text-sm">
        L
      </div>
      <span className="text-base font-black tracking-tight text-[var(--text)]">LEX CLIENT</span>
    </div>
  );
}

/** El resto (Toggle, Skeleton, Brand, Card, Badge, Button) está en el mismo archivo. */
