// Mock data only to demonstrate the UI idea. No backend involved.
export interface Subject { id: string; code: string; name: string; groupId: string }
export interface Group { id: string; name: string }
export interface ScheduleItem { subjectId: string; weekday: number; periods: string; time: string; room: string }

export const USER = { handle: 'student', fullName: 'Nguyen Van A', email: 'student@example.com' };

export const GROUPS: Group[] = [
  { id: 'g1', name: 'ITxxxx' },
  { id: 'g2', name: 'IT204x' },
  { id: 'g3', name: 'IT206x' },
];

const names = ['Programming Basics', 'Data Structures', 'Databases', 'Networks', 'Operating Systems',
  'Algorithms', 'Web Development', 'Software Engineering', 'Linear Algebra', 'Probability'];

export const SUBJECTS: Subject[] = GROUPS.flatMap((g, gi) =>
  Array.from({ length: 6 }, (_, i) => {
    const code = `IT${2021 + gi * 20 + i}`;
    return { id: code, code, name: names[(gi * 3 + i) % names.length], groupId: g.id };
  }),
);

export const SCHEDULE: ScheduleItem[] = SUBJECTS.slice(0, 10).map((s, i) => ({
  subjectId: s.id,
  weekday: 2 + (i % 6),
  periods: i % 2 ? '7-10' : '3-6',
  time: i % 2 ? '14:10 - 17:30' : '08:00 - 11:20',
  room: `50${(i % 4) + 1} D9`,
}));

export const SEMESTERS = ['20261', '20252', '20251'];
