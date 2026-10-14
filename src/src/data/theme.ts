// Color theme utilities for Noteman
export interface ThemeColor {
  id: string;
  name: string;
  accent: string;
  accentHover: string;
  bgLight: string;
  border: string;
  text: string;
}

export const THEME_PALETTES: ThemeColor[] = [
  {
    id: 'indigo',
    name: 'Indigo',
    accent: '#6366f1',
    accentHover: '#4f46e5',
    bgLight: 'rgba(99, 102, 241, 0.15)',
    border: 'rgba(99, 102, 241, 0.35)',
    text: '#818cf8',
  },
  {
    id: 'cyan',
    name: 'Cyan',
    accent: '#06b6d4',
    accentHover: '#0891b2',
    bgLight: 'rgba(6, 182, 212, 0.15)',
    border: 'rgba(6, 182, 212, 0.35)',
    text: '#22d3ee',
  },
  {
    id: 'emerald',
    name: 'Emerald',
    accent: '#10b981',
    accentHover: '#059669',
    bgLight: 'rgba(16, 185, 129, 0.15)',
    border: 'rgba(16, 185, 129, 0.35)',
    text: '#34d399',
  },
  {
    id: 'amber',
    name: 'Amber',
    accent: '#f59e0b',
    accentHover: '#d97706',
    bgLight: 'rgba(245, 158, 11, 0.15)',
    border: 'rgba(245, 158, 11, 0.35)',
    text: '#fbbf24',
  },
  {
    id: 'violet',
    name: 'Violet',
    accent: '#a855f7',
    accentHover: '#9333ea',
    bgLight: 'rgba(168, 85, 247, 0.15)',
    border: 'rgba(168, 85, 247, 0.35)',
    text: '#c084fc',
  },
  {
    id: 'rose',
    name: 'Rose',
    accent: '#f43f5e',
    accentHover: '#e11d48',
    bgLight: 'rgba(244, 63, 94, 0.15)',
    border: 'rgba(244, 63, 94, 0.35)',
    text: '#fb7185',
  },
  {
    id: 'sky',
    name: 'Sky',
    accent: '#0ea5e9',
    accentHover: '#0284c7',
    bgLight: 'rgba(14, 165, 233, 0.15)',
    border: 'rgba(14, 165, 233, 0.35)',
    text: '#38bdf8',
  },
];

export function getSubjectColor(id: string): ThemeColor {
  let hash = 0;
  for (let i = 0; i < id.length; i++) {
    hash = (hash << 5) - hash + id.charCodeAt(i);
    hash |= 0;
  }
  const index = Math.abs(hash) % THEME_PALETTES.length;
  return THEME_PALETTES[index];
}
