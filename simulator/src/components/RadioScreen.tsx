import { useAppStore } from '@/store/useAppStore';
import { AlertBanner } from './widgets/AlertBanner';
import { FrequencyDisplay } from './widgets/FrequencyDisplay';
import { LEDIndicator } from './widgets/LEDIndicator';
import { LogTerminal } from './widgets/LogTerminal';
import { MapCanvas } from './widgets/MapCanvas';
import { SpectrumCanvas } from './widgets/SpectrumCanvas';
import { VUMeter } from './widgets/VUMeter';

export function RadioScreen() {
  const view = useAppStore((s) => s.view);
  const system = useAppStore((s) => s.system);
  const radio = useAppStore((s) => s.radio);
  const asr = useAppStore((s) => s.asr);
  const analysis = useAppStore((s) => s.analysis);
  const formatFrequency = useAppStore((s) => s.formatFrequency);

  if (view !== 'radio' && view !== 'apps') return null;

  return (
    <div className="flex h-full w-full flex-col bg-neutral-950 text-neutral-200">
      <AlertBanner level={analysis.alertLevel} message={analysis.summary} />

      <div className="flex flex-1 gap-3 overflow-hidden p-3">
        {/* Left column: radio + audio + map */}
        <div className="flex w-1/3 min-w-[320px] flex-col gap-3">
          <FrequencyDisplay frequency={formatFrequency()} mode={radio.mode} />

          <div className="rounded-lg border border-neutral-700 bg-neutral-900/80 p-3">
            <div className="mb-3 flex items-center justify-between">
              <span className="text-[10px] uppercase tracking-widest text-neutral-500">System Status</span>
              <div className="flex gap-3">
                <LEDIndicator active={system.wifiConnected} color="green" label="WiFi" />
                <LEDIndicator active={system.gpsFix} color="blue" label="GPS" />
                <LEDIndicator active={radio.pttActive} color="red" label="PTT" />
              </div>
            </div>
            <div className="space-y-2">
              <MetricRow label="CPU" value={`${system.cpu}%`} />
              <MetricRow label="MEM" value={`${system.memory}%`} />
              <MetricRow label="TEMP" value={`${system.temperature}°C`} />
              {system.wifiConnected && <MetricRow label="SSID" value={system.wifiSSID} />}
            </div>
          </div>

          <div className="rounded-lg border border-neutral-700 bg-neutral-900/80 p-3">
            <VUMeter level={system.audioLevel} label="Audio Level" />
          </div>

          <div className="min-h-0 flex-1">
            <MapCanvas />
          </div>
        </div>

        {/* Center column: spectrum + ASR + analysis */}
        <div className="flex flex-1 flex-col gap-3">
          <div className="h-48 shrink-0">
            <SpectrumCanvas />
          </div>

          <div className="flex flex-1 flex-col gap-3 overflow-hidden">
            <div className="flex-1 rounded-lg border border-neutral-700 bg-black/60 p-3 font-mono text-sm">
              <div className="mb-2 flex items-center justify-between text-[10px] uppercase tracking-widest text-neutral-500">
                <span>ASR Transcript</span>
                {asr.isRecording && (
                  <span className="flex items-center gap-1 text-rose-400 animate-pulse">
                    <span className="h-1.5 w-1.5 rounded-full bg-rose-500" />
                    REC
                  </span>
                )}
              </div>
              <div className="h-full overflow-y-auto pr-2 text-neutral-300">
                {asr.transcript ? (
                  <p className="leading-relaxed">{asr.transcript}</p>
                ) : (
                  <p className="italic text-neutral-600">Press and hold PTT to simulate voice input...</p>
                )}
                {asr.partial && (
                  <p className="mt-1 text-amber-300/80">{asr.partial}</p>
                )}
              </div>
            </div>

            <div className="shrink-0 rounded-lg border border-neutral-700 bg-neutral-900/80 p-3">
              <div className="mb-1 text-[10px] uppercase tracking-widest text-neutral-500">LLM Analysis</div>
              <div className="text-sm text-neutral-200">{analysis.summary}</div>
              {analysis.location && (
                <div className="mt-1 text-xs text-emerald-400">Location: {analysis.location}</div>
              )}
            </div>
          </div>
        </div>

        {/* Right column: QSO log */}
        <div className="flex w-80 flex-col gap-3">
          <LogTerminal />
        </div>
      </div>
    </div>
  );
}

function MetricRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-center justify-between font-mono text-xs">
      <span className="text-neutral-500">{label}</span>
      <span className="text-neutral-300">{value}</span>
    </div>
  );
}
