import { useEffect } from 'react';
import { useAppStore } from '@/store/useAppStore';

const SHUTDOWN_LOGS = [
  'Stopping ASR engine...',
  'Stopping LLM analyzer...',
  'GPS receiver stopped',
  'WiFi disconnecting...',
  'Saving QSO logs...',
  'LVGL display subsystem shutdown',
  'Unmounting rootfs',
  'System halted',
];

export function ShutdownScreen() {
  const view = useAppStore((s) => s.view);
  const shutdownProgress = useAppStore((s) => s.shutdownProgress);
  const setShutdownProgress = useAppStore((s) => s.setShutdownProgress);
  const setView = useAppStore((s) => s.setView);

  useEffect(() => {
    if (view !== 'shutdown') return;

    const interval = setInterval(() => {
      setShutdownProgress(Math.min(shutdownProgress + Math.random() * 10 + 4, 100));
    }, 250);

    if (shutdownProgress >= 100) {
      clearInterval(interval);
      setTimeout(() => setView('off'), 800);
    }

    return () => clearInterval(interval);
  }, [view, shutdownProgress, setShutdownProgress, setView]);

  if (view !== 'shutdown') return null;

  const logIndex = Math.floor((shutdownProgress / 100) * SHUTDOWN_LOGS.length);

  return (
    <div className="flex h-screen w-screen flex-col items-center justify-center bg-black text-neutral-200 animate-shutdown">
      <div className="w-80">
        <div className="mb-2 flex justify-between font-mono text-[10px] text-neutral-400">
          <span>SHUTDOWN</span>
          <span>{Math.floor(shutdownProgress)}%</span>
        </div>
        <div className="h-2 w-full overflow-hidden rounded bg-neutral-800">
          <div
            className="h-full bg-rose-600 transition-all duration-300"
            style={{ width: `${shutdownProgress}%` }}
          />
        </div>
      </div>
      <div className="mt-6 h-32 w-80 overflow-hidden rounded border border-neutral-800 bg-neutral-900/50 p-3 font-mono text-[10px] text-rose-300">
        {SHUTDOWN_LOGS.slice(0, logIndex).map((log, i) => (
          <div key={i} className="truncate">{log}</div>
        ))}
        <span className="inline-block h-3 w-1.5 animate-pulse bg-rose-400" />
      </div>
    </div>
  );
}
