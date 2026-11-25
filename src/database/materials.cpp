// Content-addressed blobs and the materials that reference them.

#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/blob_path.hpp"
#include "modules/uuid.hpp"

#include <sqlite3.h>

namespace noteman::db {

using namespace detail;

// ---------------------------------------------------------------------------
// blobs
// ---------------------------------------------------------------------------

Blob Database::register_blob(const std::string &hash, std::int64_t size,
                             const std::string &mime_type,
                             const std::optional<std::int64_t> &page_count) {
    if (!modules::is_sha256_hex(hash)) {
        throw DatabaseError("invalid blob hash (expected lowercase SHA-256 hex): " + hash,
                            SQLITE_MISMATCH);
    }
    return transaction([&] {
        prepare(sql::kInsertBlob)
            .bind(1, hash)
            .bind(2, size)
            .bind(3, mime_type)
            .bind(4, page_count)
            .run();
        auto s = prepare(sql::kSelectBlobByHash);
        s.bind(1, hash);
        return read_required(s, read_blob);
    });
}

std::optional<Blob> Database::get_blob(const std::string &hash) {
    auto s = prepare(sql::kSelectBlobByHash);
    s.bind(1, hash);
    return read_one(s, read_blob);
}

std::vector<std::string> Database::delete_unreferenced_blobs() {
    auto s = prepare(sql::kDeleteUnreferencedBlobs);
    std::vector<std::string> hashes;
    while (s.step()) hashes.push_back(s.column_text(0));
    return hashes;
}

// ---------------------------------------------------------------------------
// materials
// ---------------------------------------------------------------------------

Material Database::add_material(const std::string &subject_id, const std::string &blob_hash,
                                const std::string &display_name,
                                const std::optional<std::string> &uploaded_by) {
    auto s = prepare(sql::kInsertMaterial);
    s.bind(1, modules::generate_uuid_v4())
        .bind(2, subject_id)
        .bind(3, blob_hash)
        .bind(4, display_name)
        .bind(5, uploaded_by);
    return read_required(s, read_material);
}

std::optional<Material> Database::get_material(const std::string &id) {
    auto s = prepare(sql::kSelectMaterialById);
    s.bind(1, id);
    return read_one(s, read_material);
}

std::vector<Material> Database::list_materials(const std::string &subject_id,
                                               bool include_deleted) {
    auto s = prepare(sql::kSelectMaterialsBySubject);
    s.bind(1, subject_id).bind(2, include_deleted);
    return read_all(s, read_material);
}

bool Database::rename_material(const std::string &id, const std::string &display_name) {
    prepare(sql::kRenameMaterial).bind(1, id).bind(2, display_name).run();
    return changes() > 0;
}

bool Database::soft_delete_material(const std::string &id) {
    prepare(sql::kSoftDeleteMaterial).bind(1, id).run();
    return changes() > 0;
}

bool Database::restore_material(const std::string &id) {
    prepare(sql::kRestoreMaterial).bind(1, id).run();
    return changes() > 0;
}

bool Database::delete_material(const std::string &id) {
    prepare(sql::kDeleteMaterial).bind(1, id).run();
    return changes() > 0;
}

} // namespace noteman::db
