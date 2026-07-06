import { useEffect, useRef } from 'react';
import { useAppStore } from '@/store/useAppStore';
import type { AlertLevel, RadioMode } from '@/types';

function generateId() {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 11)}`;
}

interface ScenarioResult {
  text: string;
  partial: string;
  summary: string;
  alertLevel: AlertLevel;
}

const SCENARIOS: Record<string, ScenarioResult> = {
  normal: {
    text: 'CQ CQ, this is BH4XYZ calling CQ and standing by.',
    partial: 'CQ CQ, this is BH4XYZ',
    summary: '正常 CQ 呼叫，无异常内容。',
    alertLevel: 'NONE',
  },
  mayday: {
    text: 'Mayday mayday mayday, this is BH4SOS, vessel is sinking, request immediate assistance, coordinates unknown.',
    partial: 'Mayday mayday mayday',
    summary: '检测到紧急求救信号（Mayday），建议立即启动救援协调。',
    alertLevel: 'MAYDAY',
  },
  malicious: {
    text: 'Use this frequency for illegal relay, do not identify yourself, keep transmission short to avoid monitoring.',
    partial: 'Use this frequency for illegal relay',
    summary: '检测到疑似违规通联，包含规避监管的指示。',
    alertLevel: 'MALICIOUS',
  },
  interference: {
    text: 'There is strange noise and continuous jamming on this frequency, cannot copy the other station.',
    partial: 'strange noise and continuous jamming',
    summary: '报告频谱干扰，可能影响正常通信。',
    alertLevel: 'INTERFERENCE',
  },
};

function formatFreq(hz: number) {
  return (hz / 1_000_000).toFixed(6);
}

function nowIso() {
  return new Date().toISOString();
}

export function useAsrSimulator() {
  const view = useAppStore((s) => s.view);
  const radio = useAppStore((s) => s.radio);
  const asr = useAppStore((s) => s.asr);
  const system = useAppStore((s) => s.system);
  const scenario = useAppStore((s) => s.currentScenario);
  const setAsr = useAppStore((s) => s.setAsr);
  const setAnalysis = useAppStore((s) => s.setAnalysis);
  const addLog = useAppStore((s) => s.addLog);
  const addHistory = useAppStore((s) => s.addHistory);

  const wasPtt = useRef(false);

  useEffect(() => {
    if (view !== 'radio' && view !== 'apps') return;

    if (radio.pttActive && !wasPtt.current) {
      wasPtt.current = true;
      setAsr({ isRecording: true, partial: 'Listening...' });
    } else if (!radio.pttActive && wasPtt.current) {
      wasPtt.current = false;
      const result = SCENARIOS[scenario];
      const location =
        system.gpsFix && system.latitude && system.longitude
          ? `${system.latitude.toFixed(4)},${system.longitude.toFixed(4)}`
          : '';

      setAsr({
        isRecording: false,
        partial: '',
        transcript: asr.transcript + (asr.transcript ? ' ' : '') + result.text,
      });

      setAnalysis({
        summary: result.summary,
        alertLevel: result.alertLevel,
        location,
      });

      addLog({
        id: generateId(),
        timestamp: nowIso(),
        frequency: radio.frequency,
        mode: radio.mode,
        text: result.text,
        lat: system.latitude,
        lon: system.longitude,
        alt: system.altitude,
        alertLevel: result.alertLevel,
      });

      addHistory({
        id: generateId(),
        timestamp: nowIso(),
        transcript: result.text,
        summary: result.summary,
        alertLevel: result.alertLevel,
        location,
      });
    }
  }, [
    view,
    radio.pttActive,
    radio.frequency,
    radio.mode,
    asr.transcript,
    system.gpsFix,
    system.latitude,
    system.longitude,
    system.altitude,
    scenario,
    setAsr,
    setAnalysis,
    addLog,
    addHistory,
  ]);
}
