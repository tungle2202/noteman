import { createContext, useCallback, useContext, useMemo, useState, type ReactNode } from 'react';

export interface Note { id: string; text: string; createdAt: number }
export interface Thread { id: string; title: string; description: string; author: string; createdAt: number; notes: Note[] }

interface Store {
  pinned: string[];
  recent: string[];
  threads: (subjectId: string) => Thread[];
  togglePin: (subjectId: string) => void;
  visit: (subjectId: string) => void;
  createThread: (subjectId: string, text: string, author: string) => void;
  addNote: (subjectId: string, threadId: string, text: string) => void;
}
const Ctx = createContext<Store | null>(null);
const uid = () => Math.random().toString(36).slice(2, 9);

export function StoreProvider({ children }: { children: ReactNode }) {
  const [pinned, setPinned] = useState<string[]>(['IT2021', 'IT2022']);
  const [recent, setRecent] = useState<string[]>([]);
  const [byS, setByS] = useState<Record<string, Thread[]>>({});

  const threads = useCallback((s: string) => byS[s] ?? [], [byS]);
  const togglePin = useCallback((s: string) =>
    setPinned((p) => (p.includes(s) ? p.filter((x) => x !== s) : [s, ...p])), []);
  const visit = useCallback((s: string) =>
    setRecent((r) => [s, ...r.filter((x) => x !== s)].slice(0, 8)), []);

  const createThread = useCallback((s: string, text: string, author: string) => {
    const [first, ...rest] = text.trim().split('\n');
    const th: Thread = { id: uid(), title: first, description: rest.join('\n').trim(), author, createdAt: Date.now(), notes: [] };
    setByS((m) => ({ ...m, [s]: [...(m[s] ?? []), th] }));
  }, []);

  const addNote = useCallback((s: string, threadId: string, text: string) => {
    const note: Note = { id: uid(), text: text.trim(), createdAt: Date.now() };
    setByS((m) => ({ ...m, [s]: (m[s] ?? []).map((t) => (t.id === threadId ? { ...t, notes: [...t.notes, note] } : t)) }));
  }, []);

  const value = useMemo(() => ({ pinned, recent, threads, togglePin, visit, createThread, addNote }),
    [pinned, recent, threads, togglePin, visit, createThread, addNote]);
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
}

export function useStore() {
  const c = useContext(Ctx);
  if (!c) throw new Error('useStore must be used inside StoreProvider');
  return c;
}
