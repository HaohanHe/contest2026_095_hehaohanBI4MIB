import { useAppStore } from '@/store/useAppStore';
import { LogTerminal } from './widgets/LogTerminal';
import { MapCanvas } from './widgets/MapCanvas';
import { SpectrumCanvas } from './widgets/SpectrumCanvas';
import { Settings2, Wifi, Compass, Cpu, Thermometer, Volume2 } from 'lucide-react';

export function AppContent() {
  const view = useAppStore((s) => s.view);
  const activeApp = useAppStore((s) => s.activeApp);

  if (view !== 'apps') return null;

  return (
    <div className="flex h-full w-full flex-col bg-neutral-950 p-3">
      <div className="flex-1 overflow-hidden rounded-lg border border-neutral-800 bg-black/40">
        {activeApp === 'log' && <LogApp />}
        {activeApp === 'settings' && <SettingsApp />}
        {activeApp === 'map' && <MapApp />}
        {activeApp === 'spectrum' && <SpectrumApp />}
        {activeApp === 'history' && <HistoryApp />}
      </div>
    </div>
  );
}

function LogApp() {
  return (
    <div className="h-full p-4">
      <div className="mb-3 text-xs uppercase tracking-widest text-neutral-500">JS Mini Program: QSO Logger</div>
      <LogTerminal />
    </div>
  );
}

function MapApp() {
  return (
    <div className="flex h-full flex-col p-4">
      <div className="mb-3 text-xs uppercase tracking-widest text-neutral-500">JS Mini Program: GPS Map</div>
      <div className="flex-1 min-h-0">
        <MapCanvas />
      </div>
    </div>
  );
}

function SpectrumApp() {
  return (
    <div className="flex h-full flex-col p-4">
      <div className="mb-3 text-xs uppercase tracking-widest text-neutral-500">JS Mini Program: Spectrum Analyzer</div>
      <div className="flex-1 min-h-0">
        <SpectrumCanvas />
      </div>
    </div>
  );
}

function HistoryApp() {
  const history = useAppStore((s) => s.history);

  return (
    <div className="flex h-full flex-col p-4">
      <div className="mb-3 text-xs uppercase tracking-widest text-neutral-500">JS Mini Program: Analysis History</div>
      <div className="flex-1 overflow-y-auto pr-2 font-mono text-xs">
        {history.length === 0 && <div className="italic text-neutral-600">No analysis history.</div>}
        {history.map((h) => (
          <div key={h.id} className="mb-3 border-b border-neutral-800 pb-2">
            <div className="flex items-center justify-between text-neutral-500">
              <span>{h.timestamp.slice(11, 19)}</span>
              <span className={alertColor(h.alertLevel)}>{h.alertLevel}</span>
            </div>
            <div className="mt-1 text-neutral-300">{h.transcript}</div>
            <div className="mt-1 text-neutral-400">→ {h.summary}</div>
            {h.location && <div className="mt-1 text-emerald-500">{h.location}</div>}
          </div>
        ))}
      </div>
    </div>
  );
}

function alertColor(level: string) {
  if (level === 'MAYDAY' || level === 'MALICIOUS') return 'text-rose-400';
  if (level === 'INTERFERENCE') return 'text-violet-400';
  if (level === 'WARNING') return 'text-amber-400';
  if (level === 'INFO') return 'text-sky-400';
  return 'text-neutral-500';
}

function SettingsApp() {
  const system = useAppStore((s) => s.system);
  const radio = useAppStore((s) => s.radio);
  const setSystem = useAppStore((s) => s.setSystem);

  return (
    <div className="h-full overflow-y-auto p-4">
      <div className="mb-4 text-xs uppercase tracking-widest text-neutral-500">JS Mini Program: System Settings</div>
      <div className="grid grid-cols-2 gap-3">
        <ToggleCard
          icon={Wifi}
          label="WiFi"
          active={system.wifiConnected}
          onClick={() => setSystem({ wifiConnected: !system.wifiConnected, wifiSSID: system.wifiConnected ? '' : 'AI-Radio-AP' })}
        />
        <ToggleCard
          icon={Compass}
          label="GPS Fix"
          active={system.gpsFix}
          onClick={() => setSystem({ gpsFix: !system.gpsFix })}
        />
        <StatCard icon={Cpu} label="CPU" value={`${system.cpu}%`} />
        <StatCard icon={Thermometer} label="Temperature" value={`${system.temperature}°C`} />
        <StatCard icon={Volume2} label="Audio" value={`${system.audioLevel}%`} />
        <StatCard icon={Settings2} label="Squelch" value={`${radio.squelch}`} />
      </div>
      <div className="mt-4 rounded border border-neutral-800 bg-neutral-900/60 p-3 font-mono text-[10px] text-neutral-500">
        <div>Platform: Allwinner R528 / openvela</div>
        <div>LVGL: v8.3</div>
        <div>ASR: engine ready</div>
        <div>LLM: local analyzer</div>
      </div>
    </div>
  );
}

function ToggleCard({ icon: Icon, label, active, onClick }: { icon: React.ElementType; label: string; active: boolean; onClick: () => void }) {
  return (
    <button
      onClick={onClick}
      className={`flex items-center justify-between rounded-lg border p-3 text-left transition-colors ${
        active ? 'border-emerald-500/40 bg-emerald-500/10' : 'border-neutral-700 bg-neutral-900/60'
      }`}
    >
      <div className="flex items-center gap-2">
        <Icon className={`h-4 w-4 ${active ? 'text-emerald-400' : 'text-neutral-500'}`} />
        <span className="text-xs text-neutral-300">{label}</span>
      </div>
      <div className={`h-2 w-2 rounded-full ${active ? 'bg-emerald-400 shadow-[0_0_6px] shadow-emerald-500/60' : 'bg-neutral-700'}`} />
    </button>
  );
}

function StatCard({ icon: Icon, label, value }: { icon: React.ElementType; label: string; value: string }) {
  return (
    <div className="rounded-lg border border-neutral-700 bg-neutral-900/60 p-3">
      <div className="flex items-center gap-2 text-neutral-500">
        <Icon className="h-4 w-4" />
        <span className="text-[10px] uppercase tracking-widest">{label}</span>
      </div>
      <div className="mt-1 font-mono text-lg text-neutral-200">{value}</div>
    </div>
  );
}
