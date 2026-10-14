import { ChevronLeft, ChevronRight } from 'lucide-react';
import { useI18n } from '../i18n/I18nProvider';

export function Calendar({
  view,
  onView,
  selected,
  onSelect,
  activeWeekdays = [],
}: {
  view: Date;
  onView: (d: Date) => void;
  selected: Date;
  onSelect: (d: Date) => void;
  activeWeekdays?: number[];
}) {
  const { t, lang } = useI18n();
  const today = new Date();
  const y = view.getFullYear();
  const m = view.getMonth();

  const monthName = (i: number) => new Intl.DateTimeFormat(lang, { month: 'short' }).format(new Date(2000, i, 1));
  const dow = (i: number) => new Intl.DateTimeFormat(lang, { weekday: 'short' }).format(new Date(2023, 0, 1 + i)).slice(0, 2);
  const lead = new Date(y, m, 1).getDay();
  const days = new Date(y, m + 1, 0).getDate();

  const isSelected = (d: number) =>
    selected.getFullYear() === y && selected.getMonth() === m && selected.getDate() === d;

  const isToday = (d: number) =>
    today.getFullYear() === y && today.getMonth() === m && today.getDate() === d;

  const hasEvent = (d: number) => {
    const dayOfWeek = new Date(y, m, d).getDay();
    const weekday = dayOfWeek === 0 ? 8 : dayOfWeek + 1;
    return activeWeekdays.includes(weekday);
  };

  const years = Array.from({ length: 11 }, (_, i) => new Date().getFullYear() - 5 + i);

  return (
    <div className="cal">
      <div className="cal-head">
        <button className="ibtn" aria-label={t('home.prevMonth')} onClick={() => onView(new Date(y, m - 1, 1))}>
          <ChevronLeft size={16} />
        </button>
        <select className="select" value={m} onChange={(e) => onView(new Date(y, +e.target.value, 1))}>
          {Array.from({ length: 12 }, (_, i) => (
            <option key={i} value={i}>
              {monthName(i)}
            </option>
          ))}
        </select>
        <select className="select" value={y} onChange={(e) => onView(new Date(+e.target.value, m, 1))}>
          {years.includes(y) ? years.map((v) => <option key={v}>{v}</option>) : [y, ...years].map((v) => <option key={v}>{v}</option>)}
        </select>
        <button className="ibtn" aria-label={t('home.nextMonth')} onClick={() => onView(new Date(y, m + 1, 1))}>
          <ChevronRight size={16} />
        </button>
      </div>
      <div className="cal-grid">
        {Array.from({ length: 7 }, (_, i) => (
          <div className="dow" key={i}>
            {dow(i)}
          </div>
        ))}
        {Array.from({ length: lead }, (_, i) => (
          <div key={`lead-${i}`} />
        ))}
        {Array.from({ length: days }, (_, i) => {
          const dayNum = i + 1;
          const sel = isSelected(dayNum);
          const td = isToday(dayNum);
          const ev = hasEvent(dayNum);

          const classNames = [
            'day',
            sel ? 'sel' : '',
            td ? 'is-today' : '',
            ev ? 'has-event' : '',
          ]
            .filter(Boolean)
            .join(' ');

          return (
            <button
              key={dayNum}
              className={classNames}
              onClick={() => onSelect(new Date(y, m, dayNum))}
              title={td ? t('home.today') : undefined}
            >
              {dayNum}
            </button>
          );
        })}
      </div>
    </div>
  );
}
