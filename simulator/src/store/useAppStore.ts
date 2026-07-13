import { create } from 'zustand';
import type {
  View,
  SystemState,
  RadioState,
  AsrState,
  AnalysisState,
  LogEntry,
  AnalysisHistoryEntry,
  RadioMode,
  AlertLevel,
  AppId,
} from '@/types';

export interface AppStore {
  view: View;
  bootProgress: number;
  bootLogs: string[];
  system: SystemState;
  radio: RadioState;
  asr: AsrState;
  analysis: AnalysisState;
  logs: LogEntry[];
  history: AnalysisHistoryEntry[];
  activeApp: AppId;
  shutdownProgress: number;
  currentScenario: 'normal' | 'mayday' | 'malicious' | 'interference';

  setView: (view: View) => void;
  addBootLog: (log: string) => void;
  setBootProgress: (progress: number) => void;
  setSystem: (patch: Partial<SystemState>) => void;
  setRadio: (patch: Partial<RadioState>) => void;
  setAsr: (patch: Partial<AsrState>) => void;
  setAnalysis: (patch: Partial<AnalysisState>) => void;
  addLog: (entry: LogEntry) => void;
  addHistory: (entry: AnalysisHistoryEntry) => void;
  setActiveApp: (app: AppId) => void;
  setShutdownProgress: (progress: number) => void;
  setScenario: (scenario: AppStore['currentScenario']) => void;
  appendTranscript: (text: string) => void;
  clearTranscript: () => void;
  formatFrequency: () => string;
}

const nowIso = () => new Date().toISOString();

export const useAppStore = create<AppStore>((set, get) => ({
  view: 'boot',
  bootProgress: 0,
  bootLogs: [],
  system: {
    cpu: 12,
    memory: 48,
    temperature: 42,
    wifiConnected: false,
    wifiSSID: '',
    gpsFix: false,
    latitude: null,
    longitude: null,
    altitude: null,
    audioLevel: 0,
  },
  radio: {
    frequency: 145500000,
    mode: 'USB',
    pttActive: false,
    squelch: 30,
  },
  asr: {
    transcript: '',
    partial: '',
    isRecording: false,
  },
  analysis: {
    summary: '系统就绪，等待通联信号...',
    alertLevel: 'NONE',
    location: '',
  },
  logs: [],
  history: [],
  activeApp: 'log',
  shutdownProgress: 0,
  currentScenario: 'normal',

  setView: (view) => set({ view }),
  addBootLog: (log) => set((state) => ({ bootLogs: [...state.bootLogs, `[${nowIso()}] ${log}`] })),
  setBootProgress: (bootProgress) => set({ bootProgress }),
  setSystem: (patch) => set((state) => ({ system: { ...state.system, ...patch } })),
  setRadio: (patch) => set((state) => ({ radio: { ...state.radio, ...patch } })),
  setAsr: (patch) => set((state) => ({ asr: { ...state.asr, ...patch } })),
  setAnalysis: (patch) => set((state) => ({ analysis: { ...state.analysis, ...patch } })),
  addLog: (entry) => set((state) => ({ logs: [entry, ...state.logs] })),
  addHistory: (entry) => set((state) => ({ history: [entry, ...state.history] })),
  setActiveApp: (activeApp) => set({ activeApp }),
  setShutdownProgress: (shutdownProgress) => set({ shutdownProgress }),
  setScenario: (currentScenario) => set({ currentScenario }),
  appendTranscript: (text) =>
    set((state) => ({ asr: { ...state.asr, transcript: state.asr.transcript + text } })),
  clearTranscript: () => set({ asr: { transcript: '', partial: '', isRecording: false } }),
  formatFrequency: () => {
    const hz = get().radio.frequency;
    const mhz = hz / 1_000_000;
    return mhz.toFixed(6).padStart(10, '0');
  },
}));
