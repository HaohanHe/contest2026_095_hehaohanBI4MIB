import { useEffect, useRef } from 'react';
import { useAppStore } from '@/store/useAppStore';

export function MapCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const system = useAppStore((s) => s.system);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const resize = () => {
      canvas.width = canvas.clientWidth;
      canvas.height = canvas.clientHeight;
    };
    resize();
    window.addEventListener('resize', resize);

    const draw = () => {
      const w = canvas.width;
      const h = canvas.height;
      ctx.fillStyle = '#0a0a0f';
      ctx.fillRect(0, 0, w, h);

      // Grid
      ctx.strokeStyle = '#1f1f28';
      ctx.lineWidth = 1;
      for (let x = 0; x < w; x += 40) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h);
        ctx.stroke();
      }
      for (let y = 0; y < h; y += 40) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      const cx = w / 2;
      const cy = h / 2;

      // Crosshair
      ctx.strokeStyle = '#40404a';
      ctx.beginPath();
      ctx.moveTo(cx, 0);
      ctx.lineTo(cx, h);
      ctx.moveTo(0, cy);
      ctx.lineTo(w, cy);
      ctx.stroke();

      // Position marker
      if (system.gpsFix && system.latitude && system.longitude) {
        ctx.fillStyle = '#10b981';
        ctx.shadowColor = '#10b981';
        ctx.shadowBlur = 12;
        ctx.beginPath();
        ctx.arc(cx, cy, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowBlur = 0;

        ctx.fillStyle = '#d4d4d4';
        ctx.font = '12px JetBrains Mono';
        ctx.fillText(
          `${system.latitude.toFixed(4)}, ${system.longitude.toFixed(4)}`,
          cx + 12,
          cy - 12
        );
      } else {
        ctx.fillStyle = '#737373';
        ctx.font = '14px JetBrains Mono';
        ctx.textAlign = 'center';
        ctx.fillText('NO GPS FIX', cx, cy);
      }
    };

    draw();
    const interval = setInterval(draw, 1000);

    return () => {
      window.removeEventListener('resize', resize);
      clearInterval(interval);
    };
  }, [system.gpsFix, system.latitude, system.longitude]);

  return (
    <div className="flex h-full flex-col rounded-lg border border-neutral-700 bg-black/60 p-3">
      <div className="mb-2 text-[10px] uppercase tracking-widest text-neutral-500">GPS Map</div>
      <canvas ref={canvasRef} className="flex-1 w-full rounded bg-black" />
    </div>
  );
}
