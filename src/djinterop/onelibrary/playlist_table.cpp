/*
    This file is part of libdjinterop.

    libdjinterop is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libdjinterop is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with libdjinterop.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "playlist_table.hpp"

#include <cassert>

#include <sqlite_modern_cpp.h>

#include "onelibrary_context.hpp"
#include "../util/sqlite_transaction.hpp"

namespace djinterop::onelibrary
{

namespace
{
constexpr const char* k_select_playlist =
    "SELECT playlist_id, sequenceNo, name, image_id, attribute, "
    "playlist_id_parent "
    "FROM playlist WHERE playlist_id = ?";
}  // anonymous namespace

// =========================================================================
// playlist_table
// =========================================================================

playlist_table::playlist_table(
    std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t playlist_table::add(const playlist_row& row)
{
    util::sqlite_transaction trans{context_->db};

    // Compute next id.
    int64_t next_id = 0;
    context_->db
        << "SELECT COALESCE(MAX(playlist_id), 0) + 1 FROM playlist;" >>
        next_id;

    // Compute next sequenceNo among siblings (0-based, djay Pro convention).
    int64_t next_seq = 0;
    context_->db
        << "SELECT COALESCE(MAX(sequenceNo), -1) + 1 FROM playlist "
           "WHERE playlist_id_parent = ?;"
        << row.parent_id >> next_seq;

    context_->db
        << "INSERT INTO playlist "
           "(playlist_id, sequenceNo, name, image_id, attribute, "
           "playlist_id_parent) "
           "VALUES (?, ?, ?, ?, ?, ?);"
        << next_id << next_seq << row.name << row.image_id << row.attribute
        << row.parent_id;

    trans.commit();
    return next_id;
}

std::optional<playlist_row> playlist_table::get(int64_t id) const
{
    std::optional<playlist_row> result;

    context_->db << k_select_playlist << id >> [&](
        int64_t playlist_id, int64_t sequence_no, std::string name,
        int64_t image_id, int64_t attribute, int64_t parent_id)
    {
        assert(!result);
        result = playlist_row::from_columns(
            playlist_id, sequence_no, std::move(name),
            image_id, attribute, parent_id);
    };

    return result;
}

std::vector<int64_t> playlist_table::children_of(int64_t parent_id) const
{
    std::vector<int64_t> ids;
    context_->db
        << "SELECT playlist_id FROM playlist "
           "WHERE playlist_id_parent = ? ORDER BY sequenceNo;"
        << parent_id >>
        [&](int64_t id) { ids.push_back(id); };
    return ids;
}

bool playlist_table::exists(int64_t id) const
{
    int count = 0;
    context_->db
        << "SELECT count(*) FROM playlist WHERE playlist_id = ?;" << id >>
        count;
    return count > 0;
}

// =========================================================================
// playlist_content_table
// =========================================================================

playlist_content_table::playlist_content_table(
    std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

void playlist_content_table::add(int64_t playlist_id, int64_t content_id)
{
    util::sqlite_transaction trans{context_->db};

    // Compute next sequenceNo (1-based for membership, SPEC §2.4).
    int64_t next_seq = 1;
    context_->db
        << "SELECT COALESCE(MAX(sequenceNo), 0) + 1 FROM playlist_content "
           "WHERE playlist_id = ?;"
        << playlist_id >> next_seq;

    context_->db
        << "INSERT INTO playlist_content "
           "(playlist_id, content_id, sequenceNo) VALUES (?, ?, ?);"
        << playlist_id << content_id << next_seq;

    trans.commit();
}

std::vector<int64_t> playlist_content_table::tracks_in(
    int64_t playlist_id) const
{
    std::vector<int64_t> ids;
    context_->db
        << "SELECT content_id FROM playlist_content "
           "WHERE playlist_id = ? ORDER BY sequenceNo;"
        << playlist_id >>
        [&](int64_t id) { ids.push_back(id); };
    return ids;
}

void playlist_content_table::remove(
    int64_t playlist_id, int64_t content_id)
{
    context_->db
        << "DELETE FROM playlist_content "
           "WHERE playlist_id = ? AND content_id = ?;"
        << playlist_id << content_id;
}

}  // namespace djinterop::onelibrary

// =============================================================================
// playlist_row::from_columns
// =============================================================================
namespace djinterop::onelibrary
{

playlist_row playlist_row::from_columns(
    int64_t playlist_id, int64_t sequence_no, std::string name,
    int64_t image_id, int64_t attribute, int64_t parent_id)
{
    return playlist_row{
        playlist_id,
        sequence_no,
        std::move(name),
        image_id,
        attribute,
        parent_id,
    };
}

}  // namespace djinterop::onelibrary