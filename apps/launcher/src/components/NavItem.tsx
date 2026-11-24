import { UiIcon, type IconName } from './InterfaceIcons';

export function NavItem({
  active,
  label,
  icon,
  hint,
  onClick,
}: {
  active: boolean;
  label: string;
  icon?: IconName;
  hint?: string;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      aria-current={active ? 'page' : undefined}
      className={`lex-nav-item ${active ? 'is-active' : ''}`}
    >
      <span className="lex-nav-indicator" />
      <span className="lex-nav-icon">{icon && <UiIcon name={icon} size={17} />}</span>
      <span className="lex-nav-text">
        <span className="lex-nav-label">{label}</span>
        {hint && <span className="lex-nav-hint">{hint}</span>}
      </span>
    </button>
  );
}
