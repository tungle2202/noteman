import { useEffect, useRef, useState } from 'react';
import { NavLink, Outlet, useLocation } from 'react-router-dom';
import { BookOpen, Home, User } from 'lucide-react';
import { useI18n, LANGUAGES, type Lang } from '../i18n/I18nProvider';
import { USER } from '../data/mock';

function UserMenu({ onClose }: { onClose: () => void }) {
  const { t, lang, setLang } = useI18n();
  return (
    <div className="popover" role="menu">
      <div className="menu-head"><small>{t('userMenu.config')}</small><strong>{t('userMenu.forUser', { user: USER.handle })}</strong></div>
      <div className="menu-sep" />
      <div className="menu-item"><span>{t('userMenu.username')}</span><small>{USER.fullName}</small></div>
      <div className="menu-item"><span>{t('userMenu.email')}</span><small>{USER.email}</small></div>
      <label className="menu-item">
        <span>{t('userMenu.language')}</span>
        <select value={lang} onChange={(e) => { setLang(e.target.value as Lang); onClose(); }}>
          {LANGUAGES.map((l) => <option key={l} value={l}>{t(`languages.${l}`)}</option>)}
        </select>
      </label>
    </div>
  );
}

export function AppShell() {
  const { t } = useI18n();
  const [open, setOpen] = useState(false);
  const ref = useRef<HTMLDivElement>(null);
  const { pathname } = useLocation();
  const onWork = pathname.startsWith('/worktree') || pathname.startsWith('/workspace');

  useEffect(() => {
    const close = (e: MouseEvent) => { if (!ref.current?.contains(e.target as Node)) setOpen(false); };
    document.addEventListener('mousedown', close);
    return () => document.removeEventListener('mousedown', close);
  }, []);

  return (
    <div className="shell">
      <nav className="side" aria-label="Main">
        <div id='appLogo' aria-hidden="true" tabIndex={-1}>
          <img src="/dist/VectorLogo.svg" alt="" width={27} height={27} draggable={false} tabIndex={-1} />
        </div>
        <hr />
        <NavLink to="/" end className={({ isActive }) => `tab ${isActive ? 'active' : ''}`} title={t('nav.home')} aria-label={t('nav.home')}><Home /></NavLink>
        <NavLink to="/worktree" className={`tab ${onWork ? 'active' : ''}`} title={t('nav.worktree')} aria-label={t('nav.worktree')}><BookOpen /></NavLink>
        <div className="filler" />
        <hr />
        <div ref={ref}>
          <button className="tab" onClick={() => setOpen((o) => !o)} title={t('nav.user')} aria-label={t('nav.user')} aria-expanded={open}><User /></button>
          {open && <UserMenu onClose={() => setOpen(false)} />}
        </div>
      </nav>
      <Outlet />
    </div>
  );
}
