import { useEffect, useRef } from 'react';

export function SpectrumCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

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

    const bins = 64;
    const history: number[][] = [];
    let animationId = 0;

    const draw = () => {
      const w = canvas.width;
      const h = canvas.height;
      ctx.fillStyle = '#050505';
      ctx.fillRect(0, 0, w, h);

      const row: number[] = [];
      for (let i = 0; i < bins; i++) {
        const noise = Math.random();
        const signal = Math.exp(-Math.pow(i - bins / 2, 2) / 80) * 0.6;
        row.push(Math.min(1, noise * 0.25 + signal * (0.5 + Math.random() * 0.5)));
      }
      history.unshift(row);
      if (history.length > Math.floor(h / 4)) history.pop();

      const binW = w / bins;
      for (let y = 0; y < history.length; y++) {
        for (let x = 0; x < bins; x++) {
          const val = history[y][x];
          if (val < 0.12) continue;
          let color = `rgba(6, 182, 212, ${val})`;
          if (val > 0.7) color = `rgba(251, 191, 36, ${val})`;
          if (val > 0.9) color = `rgba(244, 63, 94, ${val})`;
          ctx.fillStyle = color;
          ctx.fillRect(x * binW, y * 4, binW - 1, 3);
        }
      }

      ctx.strokeStyle = '#262626';
      ctx.beginPath();
      ctx.moveTo(0, h / 2);
      ctx.lineTo(w, h / 2);
      ctx.stroke();

      animationId = requestAnimationFrame(draw);
    };

    draw();
    return () => {
      window.removeEventListener('resize', resize);
      cancelAnimationFrame(animationId);
    };
  }, []);

  return (
    <div className="flex h-full flex-col rounded-lg border border-neutral-700 bg-black/60 p-3">
      <div className="mb-2 text-[10px] uppercase tracking-widest text-neutral-500">Spectrum</div>
      <canvas ref={canvasRef} className="flex-1 w-full rounded bg-black" />
    </div>
  );
}
