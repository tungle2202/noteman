import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { ChevronDown, ChevronRight, Clock, FolderTree, Hash, Pin } from 'lucide-react';
import { useI18n } from '../i18n/I18nProvider';
import { GROUPS, SUBJECTS, USER, type Subject } from '../data/mock';
import { getSubjectColor } from '../data/theme';
import { useStore } from '../data/store';
import { SearchBox } from '../components/SearchBox';

function Cards({ ids, empty }: { ids: string[]; empty: string }) {
  const { t } = useI18n();
  const nav = useNavigate();
  const { pinned, togglePin } = useStore();
  const subjects = ids.map((id) => SUBJECTS.find((s) => s.id === id)).filter(Boolean) as Subject[];

  if (!subjects.length) return <p className="muted" style={{ padding: '12px 0' }}>{empty}</p>;

  return (
    <div className="grid">
      {subjects.map((s) => {
        const on = pinned.includes(s.id);
        const label = t(on ? 'worktree.unpin' : 'worktree.pin');
        const theme = getSubjectColor(s.id);

        return (
          <div key={s.id} className="ws-card-container">
            <button
              className={`ws-card ${on ? 'is-pinned' : ''}`}
              onClick={() => nav(`/workspace/${s.id}`)}
              style={{
                borderTop: `3px solid ${theme.accent}`,
              }}
            >
              <h3>{s.code}</h3>
              <div className="ws-meta">
                <span>{s.name}</span>
              </div>
            </button>
            <button
              className={`pin ${on ? 'on' : ''}`}
              title={label}
              aria-label={label}
              aria-pressed={on}
              onClick={() => togglePin(s.id)}
            >
              <Pin size={15} fill={on ? 'currentColor' : 'none'} />
            </button>
          </div>
        );
      })}
    </div>
  );
}

export default function WorktreePage() {
  const { t } = useI18n();
  const nav = useNavigate();
  const { pinned, recent } = useStore();
  const [q, setQ] = useState('');
  const [closed, setClosed] = useState<Record<string, boolean>>({});
  const needle = q.trim().toLowerCase();

  return (
    <main className="main">
      <div className="header">
        <div className="header-left">
          <div className="header-title-badge">
            <FolderTree size={24} />
            <span>{t('worktree.title')}</span>
          </div>
        </div>
        <SearchBox value={q} onChange={setQ} placeholder={t('worktree.searchPlaceholder')} />
      </div>
      <hr />
      <div className="wt">
        <aside className="picker" aria-label="Channels">
          {GROUPS.map((g) => {
            const subs = SUBJECTS.filter(
              (s) => s.groupId === g.id && (!needle || `${s.code} ${s.name}`.toLowerCase().includes(needle)),
            );
            if (!subs.length) return null;
            const shut = closed[g.id];
            return (
              <div key={g.id} className="channel-group">
                <button
                  className="group-title"
                  onClick={() => setClosed((c) => ({ ...c, [g.id]: !shut }))}
                  aria-expanded={!shut}
                >
                  <span className="group-chevron">
                    {shut ? <ChevronRight size={12} /> : <ChevronDown size={12} />}
                  </span>
                  <span className="group-name">{g.name}</span>
                </button>
                {!shut && (
                  <div className="channel-list" role="list">
                    {subs.map((s) => (
                      <button
                        key={s.id}
                        className="channel-item"
                        title={`${s.code} — ${s.name}`}
                        onClick={() => nav(`/workspace/${s.id}`)}
                        role="listitem"
                      >
                        <Hash size={16} className="channel-hash" aria-hidden="true" />
                        <span className="channel-name">{s.code}</span>
                      </button>
                    ))}
                  </div>
                )}
              </div>
            );
          })}
        </aside>
        <section className="quick">
          <div className="quick-header">
            <h1>{t('worktree.works', { user: USER.handle })}</h1>
            <p>{USER.fullName} • {USER.email}</p>
          </div>
          <hr />
          <div className="section">
            <h2>
              <Pin size={18} style={{ color: 'var(--accent-amber)' }} />
              {t('worktree.pinned')}
            </h2>
            <Cards ids={pinned} empty={t('worktree.emptyPinned')} />
          </div>
          <div className="section">
            <h2>
              <Clock size={18} style={{ color: 'var(--accent-cyan)' }} />
              {t('worktree.recent')}
            </h2>
            <Cards ids={recent} empty={t('worktree.emptyRecent')} />
          </div>
        </section>
      </div>
    </main>
  );
}
