interface LEDIndicatorProps {
  active: boolean;
  color?: 'green' | 'red' | 'amber' | 'blue';
  label: string;
}

const COLOR_MAP = {
  green: 'bg-emerald-500 shadow-emerald-500/60',
  red: 'bg-rose-500 shadow-rose-500/60',
  amber: 'bg-amber-500 shadow-amber-500/60',
  blue: 'bg-sky-500 shadow-sky-500/60',
};

export function LEDIndicator({ active, color = 'green', label }: LEDIndicatorProps) {
  return (
    <div className="flex items-center gap-2">
      <div
        className={`h-2.5 w-2.5 rounded-full transition-all duration-300 ${
          active ? COLOR_MAP[color] : 'bg-neutral-700'
        } ${active ? 'shadow-[0_0_8px]' : ''}`}
      />
      <span className="text-[10px] font-mono uppercase tracking-wider text-neutral-400">
        {label}
      </span>
    </div>
  );
}
