# Noteman (frontend only)
`npm i && npm run dev`
- Routes: `/` schedule (detail table / calendar), `/worktree`, `/workspace/:id`
- i18n: JSON files in `public/locales/{vi,en}/common.json`, fetched at runtime (`src/i18n`). Add a language = add a folder + entry in `LANGUAGES`.
- Data is in-memory mock (`src/data`); no backend.
