import { createContext, useCallback, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';

export const LANGUAGES = ['vi', 'en'] as const;
export type Lang = (typeof LANGUAGES)[number];
type Dict = { [k: string]: string | Dict };
type Vars = Record<string, string | number>;

const STORAGE_KEY = 'noteman.lang';
const cache: Partial<Record<Lang, Dict>> = {};

async function loadLocale(lang: Lang): Promise<Dict> {
  if (cache[lang]) return cache[lang]!;
  const res = await fetch(`${import.meta.env.BASE_URL}locales/${lang}/common.json`);
  if (!res.ok) throw new Error(`Cannot load locale ${lang}`);
  return (cache[lang] = await res.json());
}

function lookup(dict: Dict | undefined, key: string): string | undefined {
  let cur: string | Dict | undefined = dict;
  for (const part of key.split('.')) {
    if (typeof cur !== 'object' || cur === null) return undefined;
    cur = cur[part];
  }
  return typeof cur === 'string' ? cur : undefined;
}

interface I18n {
  lang: Lang;
  setLang: (l: Lang) => void;
  t: (key: string, vars?: Vars) => string;
}
const Ctx = createContext<I18n | null>(null);

export function I18nProvider({ children }: { children: ReactNode }) {
  const saved = localStorage.getItem(STORAGE_KEY) as Lang | null;
  const [lang, setLangState] = useState<Lang>(saved && LANGUAGES.includes(saved) ? saved : 'vi');
  const [dict, setDict] = useState<Dict>();

  useEffect(() => {
    let alive = true;
    loadLocale(lang).then((d) => alive && setDict(d)).catch(console.error);
    document.documentElement.lang = lang;
    return () => { alive = false; };
  }, [lang]);

  const setLang = useCallback((l: Lang) => {
    localStorage.setItem(STORAGE_KEY, l);
    setLangState(l);
  }, []);

  const t = useCallback((key: string, vars?: Vars) => {
    const s = lookup(dict, key) ?? key;
    return vars ? s.replace(/\{\{(\w+)\}\}/g, (_, k) => String(vars[k] ?? '')) : s;
  }, [dict]);

  const value = useMemo(() => ({ lang, setLang, t }), [lang, setLang, t]);
  if (!dict) return null; // wait for the first locale file
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
}

export function useI18n() {
  const c = useContext(Ctx);
  if (!c) throw new Error('useI18n must be used inside I18nProvider');
  return c;
}
