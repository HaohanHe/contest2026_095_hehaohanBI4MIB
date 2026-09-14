import { useAppStore } from '@/store/useAppStore';
import { Power, Radio } from 'lucide-react';

export function OffScreen() {
  const setView = useAppStore((s) => s.setView);
  const setBootProgress = useAppStore((s) => s.setBootProgress);

  const powerOn = () => {
    setBootProgress(0);
    useAppStore.setState({ bootLogs: [] });
    setView('boot');
  };

  return (
    <div className="flex h-screen w-screen flex-col items-center justify-center bg-black text-neutral-700">
      <div className="mb-10 rounded-full border border-neutral-800 bg-neutral-900/30 p-8">
        <Radio className="h-16 w-16 text-neutral-800" />
      </div>
      <div className="text-center font-mono text-xs tracking-widest text-neutral-800">
        AI RADIO CONSOLE
      </div>
      <div className="mt-1 text-center font-mono text-[10px] tracking-widest text-neutral-900">
        POWERED OFF
      </div>
      <button
        onClick={powerOn}
        className="mt-10 flex h-16 w-16 items-center justify-center rounded-full border border-emerald-500/30 bg-emerald-500/10 text-emerald-500 transition-all hover:scale-105 hover:bg-emerald-500/20 hover:shadow-[0_0_30px_rgba(16,185,129,0.3)] active:scale-95"
      >
        <Power className="h-7 w-7" />
      </button>
      <span className="mt-3 text-[10px] text-neutral-600">按电源键开机</span>
    </div>
  );
}
