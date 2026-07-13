import { useCallback } from 'react';
import { useAppStore } from '@/store/useAppStore';
import type { RadioMode } from '@/types';

const MODES: RadioMode[] = ['USB', 'LSB', 'FM', 'AM', 'CW'];
const FREQ_STEP = 5000; // 5 kHz

export function useRadio() {
  const radio = useAppStore((s) => s.radio);
  const setRadio = useAppStore((s) => s.setRadio);

  const setPtt = useCallback(
    (active: boolean) => setRadio({ pttActive: active }),
    [setRadio]
  );

  const toggleMode = useCallback(() => {
    const idx = MODES.indexOf(radio.mode);
    const next = MODES[(idx + 1) % MODES.length];
    setRadio({ mode: next });
  }, [radio.mode, setRadio]);

  const adjustFrequency = useCallback(
    (direction: 1 | -1) => {
      const next = Math.max(1_000_000, radio.frequency + direction * FREQ_STEP);
      setRadio({ frequency: next });
    },
    [radio.frequency, setRadio]
  );

  const setMode = useCallback(
    (mode: RadioMode) => setRadio({ mode }),
    [setRadio]
  );

  return { radio, setPtt, toggleMode, adjustFrequency, setMode };
}
