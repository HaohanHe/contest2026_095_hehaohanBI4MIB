export type View = 'boot' | 'radio' | 'apps' | 'shutdown' | 'off';
export type RadioMode = 'USB' | 'LSB' | 'FM' | 'AM' | 'CW';
export type AlertLevel = 'NONE' | 'INFO' | 'WARNING' | 'MAYDAY' | 'INTERFERENCE' | 'MALICIOUS';
export type AppId = 'log' | 'settings' | 'map' | 'spectrum' | 'history';

export interface SystemState {
  cpu: number;
  memory: number;
  temperature: number;
  wifiConnected: boolean;
  wifiSSID: string;
  gpsFix: boolean;
  latitude: number | null;
  longitude: number | null;
  altitude: number | null;
  audioLevel: number;
}

export interface RadioState {
  frequency: number;
  mode: RadioMode;
  pttActive: boolean;
  squelch: number;
}

export interface AsrState {
  transcript: string;
  partial: string;
  isRecording: boolean;
}

export interface AnalysisState {
  summary: string;
  alertLevel: AlertLevel;
  location: string;
}

export interface LogEntry {
  id: string;
  timestamp: string;
  frequency: number;
  mode: RadioMode;
  text: string;
  lat: number | null;
  lon: number | null;
  alt: number | null;
  alertLevel: AlertLevel;
}

export interface AnalysisHistoryEntry {
  id: string;
  timestamp: string;
  transcript: string;
  summary: string;
  alertLevel: AlertLevel;
  location: string;
}
