interface FrequencyDisplayProps {
  frequency: string; // formatted MHz string
  mode: string;
}

export function FrequencyDisplay({ frequency, mode }: FrequencyDisplayProps) {
  return (
    <div className="flex flex-col items-center justify-center rounded-lg border border-neutral-700 bg-neutral-900/80 p-6 shadow-inner">
      <div className="font-orbitron text-6xl font-bold tracking-wider text-amber-400 tabular-nums">
        {frequency}
      </div>
      <div className="mt-2 flex w-full items-center justify-between">
        <span className="font-mono text-xs text-neutral-500">MHz</span>
        <span className="rounded bg-amber-500/10 px-2 py-0.5 font-mono text-sm font-bold text-amber-400">
          {mode}
        </span>
      </div>
    </div>
  );
}
