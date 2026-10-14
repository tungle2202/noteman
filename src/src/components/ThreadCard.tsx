import { CornerDownRight, FilePlus, Folder, SquarePen } from 'lucide-react';
import { useI18n } from '../i18n/I18nProvider';
import { getSubjectColor } from '../data/theme';
import type { Thread } from '../data/store';

export function ThreadCard({
  thread,
  expanded,
  onToggle,
  onAddNote,
  accentColor,
}: {
  thread: Thread;
  expanded: boolean;
  onToggle: () => void;
  onAddNote: () => void;
  accentColor?: string;
}) {
  const { t, lang } = useI18n();
  const fmt = (ts: number) =>
    new Intl.DateTimeFormat(lang, { dateStyle: 'short', timeStyle: 'short' }).format(ts);

  const authorTheme = getSubjectColor(thread.author);
  const borderAccent = accentColor || authorTheme.accent;

  return (
    <article className="thread">
      <div className="thread-by">
        <div
          className="avatar"
          style={{
            background: authorTheme.bgLight,
            color: authorTheme.text,
            borderColor: authorTheme.border,
          }}
        >
          {thread.author[0]?.toUpperCase()}
        </div>
        <span>
          {thread.author}
          <small>{t('thread.sentAt', { time: fmt(thread.createdAt) })}</small>
        </span>
      </div>
      <div className="preview" style={{ borderLeftColor: borderAccent }}>
        <h3>{thread.title || t('thread.untitled')}</h3>
        {thread.description && <p>{thread.description}</p>}
      </div>
      <div className="interact">
        <button className="reply" onClick={onToggle} aria-expanded={expanded}>
          <CornerDownRight size={16} />
          {expanded ? t('thread.hideNotes') : t('thread.viewNotes', { count: thread.notes.length })}
        </button>
        <span className="sp" />
        <button title={t('thread.actions.folder')} aria-label={t('thread.actions.folder')}>
          <Folder size={17} />
        </button>
        <button onClick={onAddNote} title={t('thread.actions.addNote')} aria-label={t('thread.actions.addNote')}>
          <FilePlus size={17} />
        </button>
        <button title={t('thread.actions.edit')} aria-label={t('thread.actions.edit')}>
          <SquarePen size={17} />
        </button>
      </div>
      {expanded && (
        <div className="notes">
          {thread.notes.map((n) => (
            <div className="note" key={n.id}>
              <small>{fmt(n.createdAt)}</small>
              {n.text}
            </div>
          ))}
        </div>
      )}
    </article>
  );
}
