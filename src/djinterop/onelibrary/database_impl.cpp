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

#include "database_impl.hpp"

#include <stdexcept>

#include <djinterop/crate.hpp>
#include <djinterop/exceptions.hpp>

#include "../util/sqlite_transaction.hpp"
#include "content_table.hpp"
#include "onelibrary_context.hpp"
#include "playlist_impl.hpp"
#include "playlist_table.hpp"
#include "track_impl.hpp"

namespace djinterop::onelibrary
{

database_impl::database_impl(std::shared_ptr<onelibrary_context> context) :
    djinterop::database_impl{
        {// OneLibrary has no crate concept.
         // feature::supports_nested_crates — NOT set.
         feature::supports_nested_playlists,
         feature::playlists_and_crates_are_distinct,
         feature::playlists_support_duplicate_tracks}},
    context_{std::move(context)}
{
}

// =============================================================================
// Crate operations — throw (OneLibrary has no crates)
// =============================================================================

std::optional<crate> database_impl::crate_by_id(int64_t)
{
    throw std::logic_error{"OneLibrary does not support crates"};
}

crate database_impl::create_root_crate(const std::string&)
{
    throw std::logic_error{"OneLibrary does not support crates"};
}

crate database_impl::create_root_crate_after(
    const std::string&, const crate&)
{
    throw std::logic_error{"OneLibrary does not support crates"};
}

void database_impl::remove_crate(crate)
{
    throw std::logic_error{"OneLibrary does not support crates"};
}

std::vector<crate> database_impl::root_crates()
{
    throw std::logic_error{"OneLibrary does not support crates"};
}

std::optional<crate> database_impl::root_crate_by_name(const std::string&)
{
    throw std::logic_error{"OneLibrary does not support crates"};
}

// =============================================================================
// Playlist operations
// =============================================================================

playlist database_impl::create_root_playlist(const std::string& name)
{
    playlist_table pt{context_};
    playlist_row row;
    row.name = name;
    row.parent_id = 0;
    auto id = pt.add(row);
    return playlist{std::make_shared<playlist_impl>(context_, id)};
}

playlist database_impl::create_root_playlist_after(
    const std::string& name, const djinterop::playlist_impl& after)
{
    // For now, just create at root. "After" ordering requires
    // sequenceNo reshuffling which is not yet implemented.
    return create_root_playlist(name);
}

std::vector<playlist> database_impl::root_playlists()
{
    playlist_table pt{context_};
    auto ids = pt.children_of(0);

    std::vector<playlist> results;
    results.reserve(ids.size());
    for (auto&& id : ids)
    {
        results.emplace_back(
            std::make_shared<playlist_impl>(context_, id));
    }
    return results;
}

std::optional<playlist> database_impl::root_playlist_by_name(
    const std::string& name)
{
    int64_t id = 0;
    context_->db
        << "SELECT playlist_id FROM playlist "
           "WHERE playlist_id_parent = 0 AND name = ?;"
        << name >> id;
    if (id == 0) return std::nullopt;

    return playlist{std::make_shared<playlist_impl>(context_, id)};
}

void database_impl::remove_playlist(const djinterop::playlist_impl& pl_base)
{
    const auto& pl = dynamic_cast<const playlist_impl&>(pl_base);
    auto id = pl.id();

    util::sqlite_transaction trans{context_->db};
    context_->db
        << "DELETE FROM playlist_content WHERE playlist_id = ?;" << id;
    context_->db << "DELETE FROM playlist WHERE playlist_id = ?;" << id;
    trans.commit();
}

// =============================================================================
// Track operations
// =============================================================================

track database_impl::create_track(const track_snapshot& snapshot)
{
    return create_track_from_snapshot(context_, snapshot);
}

std::optional<track> database_impl::track_by_id(int64_t id)
{
    content_table ct{context_};
    if (ct.get(id))
    {
        return track{std::make_shared<track_impl>(context_, id)};
    }
    return std::nullopt;
}

std::vector<track> database_impl::tracks()
{
    content_table ct{context_};
    auto ids = ct.all_ids();

    std::vector<track> results;
    results.reserve(ids.size());
    for (auto&& id : ids)
    {
        results.emplace_back(
            std::make_shared<track_impl>(context_, id));
    }
    return results;
}

std::vector<track> database_impl::tracks_by_relative_path(
    const std::string& relative_path)
{
    std::vector<track> results;
    content_table ct{context_};
    // Scan all tracks for matching path.
    for (auto id : ct.all_ids())
    {
        auto row = ct.get(id);
        if (row && row->path == relative_path)
        {
            results.emplace_back(
                std::make_shared<track_impl>(context_, id));
        }
    }
    return results;
}

void database_impl::remove_track(track tr)
{
    util::sqlite_transaction trans{context_->db};
    context_->db << "DELETE FROM content WHERE content_id = ?;" << tr.id();
    trans.commit();
}

// =============================================================================
// Misc
// =============================================================================

std::string database_impl::directory()
{
    return context_->directory;
}

void database_impl::verify()
{
    std::string result;
    context_->db << "PRAGMA integrity_check;" >> result;
    if (result != "ok")
    {
        throw database_inconsistency{
            "Database integrity check failed: " + result};
    }
}

std::string database_impl::uuid()
{
    std::string name;
    context_->db << "SELECT deviceName FROM property LIMIT 1;" >> name;
    return name;
}

std::string database_impl::version_name()
{
    return "OneLibrary 1000";
}

}  // namespace djinterop::onelibrary