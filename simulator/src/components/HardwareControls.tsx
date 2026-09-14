import { useAppStore } from '@/store/useAppStore';
import { useRadio } from '@/hooks/useRadio';
import { Mic, Plus, Minus, Power, Settings2 } from 'lucide-react';

export function HardwareControls() {
  const view = useAppStore((s) => s.view);
  const setView = useAppStore((s) => s.setView);
  const { radio, setPtt, toggleMode, adjustFrequency } = useRadio();

  const isRunning = view === 'radio' || view === 'apps';

  const startShutdown = () => {
    if (!isRunning) return;
    useAppStore.setState({ shutdownProgress: 0 });
    setView('shutdown');
  };

  return (
    <div className="flex items-center justify-between gap-3 border-t border-neutral-800 bg-neutral-950/95 px-4 py-3">
      <div className="flex items-center gap-3">
        <button
          onClick={startShutdown}
          disabled={!isRunning}
          title="关机"
          className={`flex h-10 w-10 items-center justify-center rounded-full border transition-all ${
            isRunning
              ? 'border-rose-500/30 bg-rose-500/10 text-rose-400 hover:bg-rose-500/20 hover:shadow-[0_0_16px_rgba(244,63,94,0.3)]'
              : 'border-neutral-800 bg-neutral-900 text-neutral-700'
          }`}
        >
          <Power className="h-4 w-4" />
        </button>
        <div className="h-8 w-px bg-neutral-800" />
        <HardwareButton
          onMouseDown={() => setPtt(true)}
          onMouseUp={() => setPtt(false)}
          onMouseLeave={() => setPtt(false)}
          onTouchStart={() => setPtt(true)}
          onTouchEnd={() => setPtt(false)}
          active={radio.pttActive}
          color="rose"
          label="PTT"
          icon={Mic}
        />
        <div className="mx-2 h-8 w-px bg-neutral-800" />
        <HardwareButton onClick={() => adjustFrequency(-1)} label="-" icon={Minus} />
        <HardwareButton onClick={() => adjustFrequency(1)} label="+" icon={Plus} />
        <HardwareButton onClick={toggleMode} label={radio.mode} icon={Settings2} />
      </div>

      <div className="flex items-center gap-3">
        {radio.pttActive ? (
          <span className="text-[10px] font-bold uppercase tracking-widest text-rose-400 animate-pulse">TX</span>
        ) : (
          <span className="text-[10px] uppercase tracking-widest text-neutral-600">RX</span>
        )}
      </div>
    </div>
  );
}

interface HardwareButtonProps {
  onClick?: () => void;
  onMouseDown?: () => void;
  onMouseUp?: () => void;
  onMouseLeave?: () => void;
  onTouchStart?: () => void;
  onTouchEnd?: () => void;
  active?: boolean;
  color?: 'neutral' | 'rose' | 'amber';
  label: string;
  icon: React.ElementType;
}

function HardwareButton({
  onClick,
  onMouseDown,
  onMouseUp,
  onMouseLeave,
  onTouchStart,
  onTouchEnd,
  active,
  color = 'neutral',
  label,
  icon: Icon,
}: HardwareButtonProps) {
  const colorClass =
    color === 'rose'
      ? active
        ? 'border-rose-500 bg-rose-500 text-black animate-ptt'
        : 'border-rose-500/40 bg-rose-500/10 text-rose-400 hover:bg-rose-500/20'
      : active
        ? 'border-amber-500 bg-amber-500 text-black'
        : 'border-neutral-700 bg-neutral-900 text-neutral-300 hover:border-neutral-600 hover:bg-neutral-800';

  return (
    <button
      onClick={onClick}
      onMouseDown={onMouseDown}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseLeave}
      onTouchStart={onTouchStart}
      onTouchEnd={onTouchEnd}
      className={`flex items-center gap-2 rounded-lg border px-3 py-2 text-xs font-bold uppercase tracking-wider transition-all active:scale-95 ${colorClass}`}
    >
      <Icon className="h-3.5 w-3.5" />
      {label}
    </button>
  );
}
