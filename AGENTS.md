# Agent Working Context & Memory

## 1. Tech Stack & Decisions

- Framework: Tauri + Vite (Vanilla JS/TS) + Tailwind CSS.
- Database: SQLite (local-first, managed by Rust backend like, implement idea from how package managers on linux).
- Embedded Editors: Markdown (.md) and LaTeX (.tex) with simple GUI toolbar and render engine (use KaTeX for maths render).

## 2. Current Progress (Last update: 30/09)

- Finished tasks:
  - [x] Init Tauri working environment with Vite vanilla Framework
  - [x] Deploy SQLite database base on design in ./design/database_design.json

- Current task:
  - [ ]

## 3. Rules for Agent

- **Context First**: Whenever we start a new session, read this file first to understand the context. Don't rescan the codebase unless we need to check specific code implementations.
- **Strict IPC Usage**: Always verify the `available_tauri_commands` list in `./design/dev_env.json` before writing frontend-backend communication. Do NOT invent new endpoints.
- **Storage Routing & Atomic Ops**: Every file creation or saving operation MUST strictly follow the `routing_matrix` defined in `./design/production_directory_tree.json`. Writing the file payload to disk and inserting its metadata into SQLite must be ATOMIC (if disk write fails, no DB record; if DB insert fails, clean up the uncommitted file).
- **Database Cascade**: The SQLite DB uses WAL mode and enforces foreign keys (ON DELETE CASCADE). Do not write manual Rust code to delete child records (like files or tasks) when a parent thread/subject is deleted; let SQLite handle it.
- **Quick Access to codebase overview**:
  - `./design/database_design.json` : the structure of project's database, any changes to the database logic or structure must be updated to that file.
  - `./design/dev_env.json` : the structure of the developing environment, the structure of the repository, any changes or updates related to the repository structure or developing environment must be updated to the file.
  - `./design/production_directory_tree.json` : the intended structure of storage that will be used on production. Any implementation must strictly follow this structure for a unified development direction. Any changes must be updated here.
- **State Update**: After finishing one task, tick `[x]` the Finished tasks' content in "Current Progress" to update the current state, then I will update the new task myself.
