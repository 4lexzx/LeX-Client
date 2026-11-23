import type { Screen } from '../stores/appStore';
import { UiIcon, type IconName } from './InterfaceIcons';

export function NavItem({ active, label, icon, onClick }: { active: boolean; label: string; icon?: IconName; onClick: () => void }) {
  void (null as unknown as Screen);
  return (
    <button
      onClick={onClick}
      className={`w-full text-left flex items-center gap-3 px-3 py-2.5 rounded-lg text-sm font-medium transition-all ${
        active ? 'bg-[var(--accent)] text-white' : 'text-[var(--text-secondary)] hover:text-[var(--text)] hover:bg-[var(--surface-2)]'
      }`}
    >
      <span className="w-5 text-center flex justify-center">{icon && <UiIcon name={icon} size={17} />}</span>
      <span>{label}</span>
    </button>
  );
}
