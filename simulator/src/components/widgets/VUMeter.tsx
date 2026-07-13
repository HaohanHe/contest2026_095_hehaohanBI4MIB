interface VUMeterProps {
  level: number; // 0-100
  label: string;
}

export function VUMeter({ level, label }: VUMeterProps) {
  const clamped = Math.max(0, Math.min(100, level));
  const segments = 16;
  const activeCount = Math.round((clamped / 100) * segments);

  return (
    <div className="flex flex-col gap-1">
      <span className="text-[10px] font-mono uppercase text-neutral-400">{label}</span>
      <div className="flex gap-0.5">
        {Array.from({ length: segments }).map((_, i) => {
          const active = i < activeCount;
          let colorClass = 'bg-emerald-500';
          if (i > 10) colorClass = 'bg-amber-500';
          if (i > 13) colorClass = 'bg-rose-500';
          return (
            <div
              key={i}
              className={`h-3 w-1.5 rounded-sm transition-colors duration-75 ${
                active ? colorClass : 'bg-neutral-800'
              }`}
            />
          );
        })}
      </div>
    </div>
  );
}
