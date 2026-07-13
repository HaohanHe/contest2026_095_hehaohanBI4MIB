import type { AlertLevel } from '@/types';

interface AlertBannerProps {
  level: AlertLevel;
  message: string;
}

const ALERT_STYLES: Record<AlertLevel, string> = {
  NONE: 'bg-neutral-900/60 text-neutral-400 border-neutral-700',
  INFO: 'bg-sky-900/40 text-sky-300 border-sky-600',
  WARNING: 'bg-amber-900/40 text-amber-300 border-amber-600',
  MAYDAY: 'bg-rose-900/60 text-rose-100 border-rose-500 animate-pulse-alert',
  INTERFERENCE: 'bg-violet-900/40 text-violet-300 border-violet-600',
  MALICIOUS: 'bg-rose-900/50 text-rose-200 border-rose-600 animate-pulse-alert',
};

const ALERT_LABELS: Record<AlertLevel, string> = {
  NONE: 'AI RADIO READY',
  INFO: 'INFO',
  WARNING: 'WARNING',
  MAYDAY: 'MAYDAY',
  INTERFERENCE: 'INTERFERENCE',
  MALICIOUS: 'ALERT',
};

export function AlertBanner({ level, message }: AlertBannerProps) {
  return (
    <div
      className={`w-full border-b px-4 py-2 text-center font-mono text-sm font-bold tracking-widest transition-colors ${ALERT_STYLES[level]}`}
    >
      <span className="mr-2">{ALERT_LABELS[level]}</span>
      {level !== 'NONE' && <span className="font-normal">{message}</span>}
    </div>
  );
}
