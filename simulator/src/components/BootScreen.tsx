import { useAppStore } from '@/store/useAppStore';
import { Radio } from 'lucide-react';

export function BootScreen() {
  const bootProgress = useAppStore((s) => s.bootProgress);
  const bootLogs = useAppStore((s) => s.bootLogs);

  return (
    <div className="flex h-screen w-screen flex-col items-center justify-center bg-black text-neutral-200">
      <div className="mb-8 flex flex-col items-center">
        <div className="mb-4 rounded-full border border-amber-500/30 bg-amber-500/10 p-6 shadow-[0_0_40px_rgba(245,158,11,0.2)]">
          <Radio className="h-16 w-16 text-amber-400" />
        </div>
        <h1 className="font-orbitron text-3xl font-bold tracking-[0.2em] text-amber-400">
          AI RADIO
        </h1>
        <p className="mt-2 font-mono text-xs text-neutral-500">GEMINI-S1 / R528 / openvela</p>
      </div>

      <div className="w-80">
        <div className="mb-2 flex justify-between font-mono text-[10px] text-neutral-400">
          <span>BOOT</span>
          <span>{Math.floor(bootProgress)}%</span>
        </div>
        <div className="h-2 w-full overflow-hidden rounded bg-neutral-800">
          <div
            className="h-full bg-amber-500 transition-all duration-300"
            style={{ width: `${bootProgress}%` }}
          />
        </div>
      </div>

      <div className="mt-8 h-40 w-80 overflow-hidden rounded border border-neutral-800 bg-neutral-900/50 p-3 font-mono text-[10px] text-emerald-400">
        {bootLogs.slice(-8).map((log, i) => (
          <div key={i} className="truncate">
            {log}
          </div>
        ))}
        <span className="inline-block h-3 w-1.5 animate-pulse bg-emerald-400" />
      </div>
    </div>
  );
}
