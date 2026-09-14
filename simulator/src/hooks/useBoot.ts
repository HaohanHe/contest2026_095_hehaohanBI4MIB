import { useEffect } from 'react';
import { useAppStore } from '@/store/useAppStore';

const BOOT_LOGS = [
  'BIOS: Allwinner R528 reset',
  'DRAM: 128 MB OK',
  'NAND: 256 MB OK',
  'Loading openvela kernel...',
  'Mounting rootfs',
  'Starting syslogd',
  'Initializing LVGL display subsystem',
  'Initializing audio capture (I2S2)',
  'Initializing LRADC buttons',
  'Loading WiFi driver (RTL8273)',
  'WiFi auto-connect started',
  'GPS receiver started',
  'ASR engine started',
  'LLM analyzer ready',
  'AI Radio Console v1.0 ready',
];

export function useBoot() {
  const view = useAppStore((s) => s.view);
  const bootProgress = useAppStore((s) => s.bootProgress);
  const addBootLog = useAppStore((s) => s.addBootLog);
  const setBootProgress = useAppStore((s) => s.setBootProgress);
  const setView = useAppStore((s) => s.setView);

  useEffect(() => {
    if (view !== 'boot') return;

    const interval = setInterval(() => {
      setBootProgress(Math.min(bootProgress + Math.random() * 8 + 2, 100));
      const logIndex = Math.floor((bootProgress / 100) * BOOT_LOGS.length);
      if (BOOT_LOGS[logIndex]) {
        addBootLog(BOOT_LOGS[logIndex]);
      }
    }, 300);

    if (bootProgress >= 100) {
      clearInterval(interval);
      addBootLog('Boot complete. Switching to main UI...');
      setTimeout(() => setView('radio'), 600);
    }

    return () => clearInterval(interval);
  }, [view, bootProgress, addBootLog, setBootProgress, setView]);
}
