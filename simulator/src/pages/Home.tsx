import { useAppStore } from '@/store/useAppStore';
import { useBoot } from '@/hooks/useBoot';
import { useSystemMonitor } from '@/hooks/useSystemMonitor';
import { useAsrSimulator } from '@/hooks/useAsrSimulator';
import { BootScreen } from '@/components/BootScreen';
import { RadioScreen } from '@/components/RadioScreen';
import { AppContent } from '@/components/AppContent';
import { AppDock } from '@/components/AppDock';
import { HardwareControls } from '@/components/HardwareControls';
import { ScenarioPanel } from '@/components/ScenarioPanel';
import { ShutdownScreen } from '@/components/ShutdownScreen';
import { OffScreen } from '@/components/OffScreen';

export default function Home() {
  const view = useAppStore((s) => s.view);

  useBoot();
  useSystemMonitor();
  useAsrSimulator();

  return (
    <div className="relative flex h-screen w-screen flex-col overflow-hidden bg-black">
      {/* Global CRT overlay */}
      <div className="crt-overlay" />

      {/* Main display area */}
      <div className="relative flex-1 overflow-hidden border-x-[14px] border-t-[14px] border-b-[10px] border-neutral-900 bezel">
        {view === 'off' && <OffScreen />}
        {view === 'boot' && <BootScreen />}
        {view === 'radio' && <RadioScreen />}
        {view === 'apps' && <AppContent />}
        {view === 'shutdown' && <ShutdownScreen />}
      </div>

      {/* Bottom control strips */}
      {view !== 'off' && view !== 'boot' && view !== 'shutdown' && (
        <>
          <ScenarioPanel />
          <AppDock />
          <HardwareControls />
        </>
      )}

      {/* Bezel label */}
      <div className="pointer-events-none absolute bottom-2 left-6 z-10 text-[9px] font-mono uppercase tracking-[0.3em] text-neutral-600">
        AI Radio Console · GEMINI-S1 · R528
      </div>
    </div>
  );
}
