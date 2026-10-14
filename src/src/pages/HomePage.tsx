import { useMemo, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { CalendarDays, Clock, MapPin } from 'lucide-react';
import { useI18n } from '../i18n/I18nProvider';
import { SEMESTERS, SCHEDULE, SUBJECTS } from '../data/mock';
import { getSubjectColor } from '../data/theme';
import { SearchBox } from '../components/SearchBox';
import { Calendar } from '../components/Calendar';

type Mode = 'detail' | 'calendar';
const weekdayOf = (d: Date) => (d.getDay() === 0 ? 8 : d.getDay() + 1); // Mon=2 … Sun=8

export default function HomePage() {
  const { t } = useI18n();
  const nav = useNavigate();
  const [mode, setMode] = useState<Mode>('calendar');
  const [semester, setSemester] = useState(SEMESTERS[0]);
  const [q, setQ] = useState('');
  const today = new Date();
  const [selected, setSelected] = useState(today);
  const [view, setView] = useState(new Date(today.getFullYear(), today.getMonth(), 1));

  const items = useMemo(() => {
    const needle = q.trim().toLowerCase();
    return SCHEDULE.map((s) => ({ ...s, subject: SUBJECTS.find((x) => x.id === s.subjectId)! }))
      .filter((i) => !needle || `${i.subject.code} ${i.subject.name} ${i.room}`.toLowerCase().includes(needle))
      .filter((i) => mode === 'detail' || i.weekday === weekdayOf(selected));
  }, [q, mode, selected]);

  const activeWeekdays = useMemo(() => {
    return Array.from(new Set(SCHEDULE.map((s) => s.weekday)));
  }, []);

  const open = (id: string) => nav(`/workspace/${id}`);

  return (
    <main className="main">
      <div className="header">
        <div className="header-left">
          <div className="header-title-badge">
            <CalendarDays size={24} />
            <span>{t('home.title')}</span>
          </div>
          <label style={{ display: 'flex', gap: 8, alignItems: 'center', fontSize: 13, color: 'var(--text-secondary)' }}>
            <span>{t('home.semester')}:</span>
            <select className="select" value={semester} onChange={(e) => setSemester(e.target.value)}>
              {SEMESTERS.map((s) => (
                <option key={s}>{s}</option>
              ))}
            </select>
          </label>
        </div>
        <SearchBox value={q} onChange={setQ} placeholder={t('home.searchPlaceholder')} />
      </div>
      <hr />
      <div className="modes" role="tablist">
        {(['detail', 'calendar'] as Mode[]).map((m) => (
          <button
            key={m}
            role="tab"
            aria-selected={mode === m}
            className={`mode ${mode === m ? 'on' : ''}`}
            onClick={() => setMode(m)}
          >
            {t(`home.view.${m}`)}
          </button>
        ))}
      </div>

      {mode === 'detail' ? (
        <div className="home-body">
          <div className="table">
            <div className="row hd">
              <div>{t('home.table.index')}</div>
              <div>{t('home.table.code')}</div>
              <div>{t('home.table.name')}</div>
              <div>{t('home.table.schedule')}</div>
            </div>
            {items.map((i, n) => {
              const theme = getSubjectColor(i.subject.id);
              return (
                <button className="row" key={i.subjectId} onClick={() => open(i.subjectId)}>
                  <div style={{ color: 'var(--text-muted)', fontWeight: 600 }}>{n + 1}</div>
                  <div>
                    <span
                      className="course-badge"
                      style={{ background: theme.bgLight, color: theme.text, border: `1px solid ${theme.border}` }}
                    >
                      {i.subject.code}
                    </span>
                  </div>
                  <div style={{ fontWeight: 600 }}>{i.subject.name}</div>
                  <div className="course-meta-tags">
                    <span className="meta-chip">
                      {t(`weekday.${i.weekday}`)}
                    </span>
                    <span className="meta-chip time">
                      <Clock size={12} />
                      {i.time}
                    </span>
                    <span className="meta-chip room">
                      <MapPin size={12} />
                      {i.room}
                    </span>
                  </div>
                </button>
              );
            })}
            {!items.length && <div className="empty">{t('home.empty')}</div>}
          </div>
        </div>
      ) : (
        <div className="home-body">
          <Calendar
            view={view}
            onView={setView}
            selected={selected}
            activeWeekdays={activeWeekdays}
            onSelect={(d) => {
              setSelected(d);
              setView(new Date(d.getFullYear(), d.getMonth(), 1));
            }}
          />
          <div className="cards">
            {items.map((i) => {
              const theme = getSubjectColor(i.subject.id);
              return (
                <button className="course" key={i.subjectId} onClick={() => open(i.subjectId)}>
                  <div className="course-top">
                    <div className="course-title-group">
                      <span
                        className="course-badge"
                        style={{ background: theme.bgLight, color: theme.text, border: `1px solid ${theme.border}` }}
                      >
                        {i.subject.code}
                      </span>
                      <span>{i.subject.name}</span>
                    </div>
                  </div>
                  <div className="course-meta-tags">
                    <span className="meta-chip">
                      {t(`weekday.${i.weekday}`)} (Tiết {i.periods})
                    </span>
                    <span className="meta-chip time">
                      <Clock size={12} />
                      {i.time}
                    </span>
                    <span className="meta-chip room">
                      <MapPin size={12} />
                      {i.room}
                    </span>
                  </div>
                </button>
              );
            })}
            {!items.length && <div className="empty">{t('home.empty')}</div>}
          </div>
        </div>
      )}
    </main>
  );
}
