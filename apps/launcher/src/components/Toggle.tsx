import { Toggle as BaseToggle } from '@lex/ui-components';

export function Toggle({ on, onChange, label }: { on: boolean; onChange: (v: boolean) => void; label?: string }) {
  return <BaseToggle on={on} onChange={onChange} label={label} />;
}
