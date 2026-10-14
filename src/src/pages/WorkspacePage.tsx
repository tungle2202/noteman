import { useEffect, useRef, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { ChevronRight, FolderTree, Pin } from 'lucide-react';
import { useI18n } from '../i18n/I18nProvider';
import { GROUPS, SUBJECTS } from '../data/mock';
import { getSubjectColor } from '../data/theme';
import { useStore } from '../data/store';
import { SearchBox } from '../components/SearchBox';
import { ThreadCard } from '../components/ThreadCard';
import { Composer } from '../components/Composer';
import { USER } from "../data/mock";

export default function WorkspacePage() {
  const { t } = useI18n();
  const { id = '' } = useParams();
  const { threads, visit, createThread, addNote, pinned, togglePin } = useStore();
  const [q, setQ] = useState('');
  const [open, setOpen] = useState<string | null>(null);
  const [target, setTarget] = useState<string | null>(null);
  const endRef = useRef<HTMLDivElement>(null);
  const subject = SUBJECTS.find((s) => s.id === id);
  const list = threads(id);

  useEffect(() => {
    if (subject) visit(subject.id);
  }, [subject, visit]);

  useEffect(() => {
    endRef.current?.scrollIntoView({ block: 'end' });
  }, [list.length]);

  if (!subject) {
    return (
      <main className="main">
        <p className="empty">
          {t('workspace.notFound')}{' '}
          <Link to="/worktree" style={{ color: 'var(--accent-primary)', textDecoration: 'underline' }}>
            ← {t('workspace.backToWorktree')}
          </Link>
        </p>
      </main>
    );
  }

  const group = GROUPS.find((g) => g.id === subject.groupId)!;
  const theme = getSubjectColor(subject.id);
  const needle = q.trim().toLowerCase();
  const shown = list.filter(
    (th) =>
      !needle ||
      `${th.title} ${th.description} ${th.notes.map((n) => n.text).join(' ')}`
        .toLowerCase()
        .includes(needle),
  );
  const targetThread = list.find((th) => th.id === target);
  const isPinned = pinned.includes(subject.id);
  const pinLabel = t(isPinned ? 'worktree.unpin' : 'worktree.pin');

  return (
    <main className="main">
      <div className="header">
        <div className="crumb-container">
          <Link to="/worktree" className="crumb-pill">
            <FolderTree size={14} />
            <span>{t('workspace.backToWorktree')}</span>
          </Link>
          <ChevronRight size={14} className="crumb-sep" />
          {USER.handle}/{group.name}/{subject.code}
          <span
            className="crumb-pill active"
            style={{
              background: theme.bgLight,
              color: theme.text,
              borderColor: theme.border,
            }}
          >
            {subject.name}
          </span>
          <button
            className={`crumb-pin ${isPinned ? 'on' : ''}`}
            title={pinLabel}
            aria-label={pinLabel}
            aria-pressed={isPinned}
            onClick={() => togglePin(subject.id)}
          >
            <Pin size={15} fill={isPinned ? 'currentColor' : 'none'} />
          </button>
        </div>
        <SearchBox value={q} onChange={setQ} placeholder={t('workspace.searchPlaceholder')} />
      </div>
      <hr />
      <div className="threads">
        {!list.length && <p className="empty">{t('workspace.noThreads')}</p>}
        {shown.map((th) => (
          <ThreadCard
            key={th.id}
            thread={th}
            expanded={open === th.id}
            accentColor={theme.accent}
            onToggle={() => setOpen(open === th.id ? null : th.id)}
            onAddNote={() => {
              setTarget(th.id);
              setOpen(th.id);
            }}
          />
        ))}
        <div ref={endRef} />
      </div>
      <Composer
        targetTitle={targetThread ? targetThread.title : undefined}
        onCancelTarget={() => setTarget(null)}
        onSubmit={(text) => {
          if (targetThread) addNote(id, targetThread.id, text);
          else createThread(id, text, 'student');
        }}
      />
    </main>
  );
}
