import { useAppStore } from '@/store/useAppStore';
import type { AppStore } from '@/store/useAppStore';
import { Radio, AlertTriangle, ShieldAlert, RadioTower, FileCheck } from 'lucide-react';

type Scenario = AppStore['currentScenario'];

const SCENARIOS: { id: Scenario; label: string; icon: React.ElementType; color: string }[] = [
  { id: 'normal', label: '正常 CQ', icon: Radio, color: 'text-emerald-400' },
  { id: 'mayday', label: 'Mayday 求救', icon: AlertTriangle, color: 'text-rose-400' },
  { id: 'malicious', label: '违规通联', icon: ShieldAlert, color: 'text-rose-400' },
  { id: 'interference', label: '频谱干扰', icon: RadioTower, color: 'text-violet-400' },
];

export function ScenarioPanel() {
  const view = useAppStore((s) => s.view);
  const scenario = useAppStore((s) => s.currentScenario);
  const setScenario = useAppStore((s) => s.setScenario);
  const clearTranscript = useAppStore((s) => s.clearTranscript);

  if (view !== 'radio' && view !== 'apps') return null;

  return (
    <div className="border-t border-neutral-800 bg-neutral-950/95 px-4 py-2">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span className="text-[10px] uppercase tracking-widest text-neutral-500">场景注入</span>
          {SCENARIOS.map((s) => (
            <button
              key={s.id}
              onClick={() => setScenario(s.id)}
              className={`flex items-center gap-1.5 rounded px-2 py-1 text-[10px] font-medium uppercase tracking-wide transition-colors ${
                scenario === s.id
                  ? 'bg-neutral-800 text-neutral-200'
                  : 'text-neutral-600 hover:bg-neutral-900 hover:text-neutral-400'
              }`}
            >
              <s.icon className={`h-3 w-3 ${scenario === s.id ? s.color : 'text-neutral-600'}`} />
              {s.label}
            </button>
          ))}
        </div>
        <button
          onClick={clearTranscript}
          className="flex items-center gap-1 text-[10px] text-neutral-500 hover:text-neutral-300"
        >
          <FileCheck className="h-3 w-3" />
          清空识别
        </button>
      </div>
    </div>
  );
}
