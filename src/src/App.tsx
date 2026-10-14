import { Route, Routes } from 'react-router-dom';
import { AppShell } from './components/AppShell';
import HomePage from './pages/HomePage';
import WorktreePage from './pages/WorktreePage';
import WorkspacePage from './pages/WorkspacePage';

export default function App() {
  return (
    <Routes>
      <Route element={<AppShell />}>
        <Route index element={<HomePage />} />
        <Route path="worktree" element={<WorktreePage />} />
        <Route path="workspace/:id" element={<WorkspacePage />} />
      </Route>
    </Routes>
  );
}
