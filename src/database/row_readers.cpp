#include "database/row_readers.hpp"

namespace noteman::db::detail {

User read_user(const Statement &s) {
    User u;
    u.id = s.column_text(0);
    u.username = s.column_text(1);
    u.email = s.column_optional_text(2);
    u.password_hash = s.column_optional_text(3);
    u.primary_group_id = s.column_text(4);
    u.is_admin = s.column_bool(5);
    u.created_at = s.column_text(6);
    return u;
}

Group read_group(const Statement &s) {
    return Group{s.column_text(0), s.column_text(1), s.column_text(2)};
}

Subject read_subject(const Statement &s) {
    Subject x;
    x.id = s.column_text(0);
    x.code = s.column_text(1);
    x.name = s.column_text(2);
    x.semester = s.column_text(3);
    x.owner_id = s.column_text(4);
    x.group_id = s.column_text(5);
    x.mode = static_cast<int>(s.column_int64(6));
    x.created_at = s.column_text(7);
    x.last_update = s.column_text(8);
    return x;
}

Thread read_thread(const Statement &s) {
    Thread t;
    t.id = s.column_text(0);
    t.subject_id = s.column_text(1);
    t.title = s.column_text(2);
    t.description = s.column_optional_text(3);
    t.created_by = s.column_optional_text(4);
    t.created_at = s.column_text(5);
    t.last_update = s.column_text(6);
    return t;
}

Blob read_blob(const Statement &s) {
    Blob b;
    b.hash = s.column_text(0);
    b.size = s.column_int64(1);
    b.mime_type = s.column_text(2);
    b.page_count = s.column_optional_int64(3);
    b.created_at = s.column_text(4);
    return b;
}

Material read_material(const Statement &s) {
    Material m;
    m.id = s.column_text(0);
    m.subject_id = s.column_text(1);
    m.blob_hash = s.column_text(2);
    m.display_name = s.column_text(3);
    m.uploaded_by = s.column_optional_text(4);
    m.uploaded_at = s.column_text(5);
    m.deleted_at = s.column_optional_text(6);
    return m;
}

Quote read_quote(const Statement &s) {
    Quote q;
    q.id = s.column_text(0);
    q.thread_id = s.column_text(1);
    q.material_id = s.column_text(2);
    q.kind = quote_kind_from_string(s.column_text(3));
    q.spec_json = s.column_text(4);
    q.extracted_text = s.column_optional_text(5);
    q.note = s.column_optional_text(6);
    q.author_id = s.column_optional_text(7);
    q.created_at = s.column_text(8);
    q.last_update = s.column_text(9);
    return q;
}

ReadingProgress read_reading_progress(const Statement &s) {
    ReadingProgress p;
    p.user_id = s.column_text(0);
    p.material_id = s.column_text(1);
    p.page = s.column_int64(2);
    p.page_offset = s.column_double(3);
    p.zoom = s.column_optional_double(4);
    p.updated_at = s.column_text(5);
    return p;
}

Note read_note(const Statement &s) {
    Note n;
    n.id = s.column_text(0);
    n.thread_id = s.column_text(1);
    n.file_name = s.column_text(2);
    n.format = note_format_from_string(s.column_text(3));
    n.current_version_id = s.column_optional_text(4);
    n.created_by = s.column_optional_text(5);
    n.created_at = s.column_text(6);
    n.last_update = s.column_text(7);
    return n;
}

NoteVersion read_note_version(const Statement &s) {
    NoteVersion v;
    v.id = s.column_text(0);
    v.note_id = s.column_text(1);
    v.parent_id = s.column_optional_text(2);
    v.author_id = s.column_optional_text(3);
    v.compression = compression_from_string(s.column_text(4));
    v.content_hash = s.column_text(5);
    v.size = s.column_int64(6);
    v.message = s.column_optional_text(7);
    v.pinned = s.column_bool(8);
    v.created_at = s.column_text(9);
    return v;
}

Task read_task(const Statement &s) {
    Task t;
    t.id = s.column_text(0);
    t.thread_id = s.column_text(1);
    t.title = s.column_text(2);
    t.deadline = s.column_optional_text(3);
    t.completed_at = s.column_optional_text(4);
    t.created_by = s.column_optional_text(5);
    t.created_at = s.column_text(6);
    t.last_update = s.column_text(7);
    return t;
}

} // namespace noteman::db::detail
