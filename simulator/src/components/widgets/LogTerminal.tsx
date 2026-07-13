import { useAppStore } from '@/store/useAppStore';

export function LogTerminal() {
  const logs = useAppStore((s) => s.logs);

  return (
    <div className="flex h-full flex-col rounded-lg border border-neutral-700 bg-black/60 p-3 font-mono text-xs">
      <div className="mb-2 text-[10px] uppercase tracking-widest text-neutral-500">QSO Log</div>
      <div className="flex-1 overflow-y-auto pr-2">
        <div className="space-y-1.5">
          {logs.length === 0 && (
            <div className="italic text-neutral-600">No QSO logged yet.</div>
          )}
          {logs.map((log) => (
            <div key={log.id} className="break-words text-neutral-300">
              <span className="text-neutral-500">{log.timestamp.slice(11, 19)}</span>
              <span className="mx-1 text-amber-500">{(log.frequency / 1_000_000).toFixed(3)}</span>
              <span className="text-neutral-500">{log.mode}</span>
              <span className="mx-1 text-sky-400">{log.alertLevel !== 'NONE' ? `[${log.alertLevel}]` : ''}</span>
              <span className="text-neutral-200">{log.text}</span>
              {log.lat && log.lon && (
                <span className="ml-1 text-emerald-500">
                  [{log.lat.toFixed(4)},{log.lon.toFixed(4)}]
                </span>
              )}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
