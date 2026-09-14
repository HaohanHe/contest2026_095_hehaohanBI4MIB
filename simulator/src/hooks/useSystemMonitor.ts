import { useEffect, useRef } from 'react';
import { useAppStore } from '@/store/useAppStore';

export function useSystemMonitor() {
  const view = useAppStore((s) => s.view);
  const pttActive = useAppStore((s) => s.radio.pttActive);
  const system = useAppStore((s) => s.system);
  const setSystem = useAppStore((s) => s.setSystem);

  const baseLat = useRef(31.2304 + (Math.random() - 0.5) * 0.01);
  const baseLon = useRef(121.4737 + (Math.random() - 0.5) * 0.01);

  useEffect(() => {
    if (view === 'off' || view === 'boot') return;

    const interval = setInterval(() => {
      const cpu = Math.min(
        95,
        Math.max(5, Math.floor(15 + Math.random() * 30 + (pttActive ? 25 : 0)))
      );
      const memory = Math.floor(45 + Math.random() * 25);
      const temperature = Math.floor(42 + Math.random() * 16 + (pttActive ? 5 : 0));
      const audioLevel = pttActive ? Math.floor(40 + Math.random() * 55) : Math.floor(Math.random() * 8);

      const patch: Partial<typeof system> = {
        cpu,
        memory,
        temperature,
        audioLevel,
      };

      if (system.wifiConnected) {
        patch.wifiSSID = system.wifiSSID || 'AI-Radio-AP';
      }

      if (system.gpsFix) {
        patch.latitude = baseLat.current + (Math.random() - 0.5) * 0.0002;
        patch.longitude = baseLon.current + (Math.random() - 0.5) * 0.0002;
        patch.altitude = 12 + (Math.random() - 0.5) * 3;
      }

      setSystem(patch);
    }, 800);

    return () => clearInterval(interval);
  }, [view, pttActive, system.wifiConnected, system.gpsFix, system.wifiSSID, setSystem]);
}
