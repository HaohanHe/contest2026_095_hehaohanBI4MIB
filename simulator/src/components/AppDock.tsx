import { useAppStore } from '@/store/useAppStore';
import type { AppId } from '@/types';
import { Radio, ScrollText, Settings, Map as MapIcon, Activity, History } from 'lucide-react';

const APPS: { id: AppId; label: string; icon: React.ElementType }[] = [
  { id: 'log', label: '日志', icon: ScrollText },
  { id: 'settings', label: '设置', icon: Settings },
  { id: 'map', label: '地图', icon: MapIcon },
  { id: 'spectrum', label: '频谱', icon: Activity },
  { id: 'history', label: '历史', icon: History },
];

export function AppDock() {
  const view = useAppStore((s) => s.view);
  const activeApp = useAppStore((s) => s.activeApp);
  const setActiveApp = useAppStore((s) => s.setActiveApp);
  const setView = useAppStore((s) => s.setView);

  if (view !== 'radio' && view !== 'apps') return null;

  const switchApp = (id: AppId) => {
    setActiveApp(id);
    if (view === 'radio') setView('apps');
  };

  const backToRadio = () => setView('radio');

  return (
    <div className="flex items-center justify-between gap-3 border-t border-neutral-800 bg-neutral-950/95 px-4 py-2">
      <div className="flex items-center gap-1">
        <button
          onClick={backToRadio}
          className={`flex items-center gap-2 rounded-md px-3 py-2 text-xs font-medium transition-colors ${
            view === 'radio'
              ? 'bg-amber-500/20 text-amber-400'
              : 'text-neutral-500 hover:bg-neutral-900 hover:text-neutral-300'
          }`}
        >
          <Radio className="h-4 w-4" />
          电台
        </button>
        <div className="mx-2 h-4 w-px bg-neutral-800" />
        {APPS.map((app) => (
          <button
            key={app.id}
            onClick={() => switchApp(app.id)}
            className={`flex items-center gap-2 rounded-md px-3 py-2 text-xs font-medium transition-colors ${
              view === 'apps' && activeApp === app.id
                ? 'bg-sky-500/20 text-sky-400'
                : 'text-neutral-500 hover:bg-neutral-900 hover:text-neutral-300'
            }`}
          >
            <app.icon className="h-4 w-4" />
            {app.label}
          </button>
        ))}
      </div>
      <div className="text-[10px] font-mono uppercase tracking-widest text-neutral-600">
        {view === 'radio' ? '主界面' : `小程序: ${APPS.find((a) => a.id === activeApp)?.label}`}
      </div>
    </div>
  );
}
