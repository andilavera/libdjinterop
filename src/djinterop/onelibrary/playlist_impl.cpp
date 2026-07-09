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

#include "playlist_impl.hpp"

#include <stdexcept>

#include <djinterop/exceptions.hpp>
#include <djinterop/track.hpp>

#include "../util/sqlite_transaction.hpp"
#include "database_impl.hpp"
#include "onelibrary_context.hpp"
#include "playlist_table.hpp"
#include "track_impl.hpp"

namespace djinterop::onelibrary
{

playlist_impl::playlist_impl(
    std::shared_ptr<onelibrary_context> context, int64_t id) :
    context_{std::move(context)}, id_{id}
{
}

void playlist_impl::add_track_back(const djinterop::track_impl& tr)
{
    playlist_content_table pct{context_};
    pct.add(id_, tr.id());
}

void playlist_impl::add_track_after(
    const djinterop::track_impl& tr, const djinterop::track_impl& after)
{
    // OneLibrary's playlist_content has simple sequenceNo ordering.
    // To add after, we just add (sequenceNo auto-assigned at end).
    // We could reorder, but for simplicity just add to end.
    add_track_back(tr);
}

std::vector<playlist> playlist_impl::children()
{
    playlist_table pt{context_};
    auto ids = pt.children_of(id_);

    std::vector<playlist> results;
    results.reserve(ids.size());
    for (auto&& child_id : ids)
    {
        results.emplace_back(
            std::make_shared<playlist_impl>(context_, child_id));
    }
    return results;
}

void playlist_impl::clear_tracks()
{
    util::sqlite_transaction trans{context_->db};
    context_->db
        << "DELETE FROM playlist_content WHERE playlist_id = ?;" << id_;
    trans.commit();
}

playlist playlist_impl::create_sub_playlist(const std::string& name)
{
    playlist_table pt{context_};
    playlist_row row;
    row.name = name;
    row.parent_id = id_;
    row.attribute = 0;  // playlist, not folder
    auto new_id = pt.add(row);
    return playlist{std::make_shared<playlist_impl>(context_, new_id)};
}

playlist playlist_impl::create_sub_playlist_after(
    const std::string& name, const djinterop::playlist_impl& after_base)
{
    const auto& after = dynamic_cast<const playlist_impl&>(after_base);
    // For now, just create a sub-playlist. The "after" ordering is not
    // implemented since OneLibrary uses simple sequenceNo ordering and
    // we would need to reshuffle.
    return create_sub_playlist(name);
}

database playlist_impl::db() const
{
    return database{std::make_shared<database_impl>(context_)};
}

std::string playlist_impl::name() const
{
    playlist_table pt{context_};
    auto row = pt.get(id_);
    if (!row)
        throw playlist_deleted{id_};
    return row->name;
}

std::optional<playlist> playlist_impl::parent()
{
    playlist_table pt{context_};
    auto row = pt.get(id_);
    if (!row || row->parent_id == 0)
        return std::nullopt;

    return playlist{
        std::make_shared<playlist_impl>(context_, row->parent_id)};
}

void playlist_impl::remove_track(const djinterop::track_impl& tr)
{
    playlist_content_table pct{context_};
    pct.remove(id_, tr.id());
}

void playlist_impl::set_name(const std::string& name)
{
    util::sqlite_transaction trans{context_->db};
    context_->db
        << "UPDATE playlist SET name = ? WHERE playlist_id = ?;"
        << name << id_;
    trans.commit();
}

void playlist_impl::set_parent(
    const djinterop::playlist_impl* parent_base_maybe)
{
    int64_t new_parent_id = 0;
    if (parent_base_maybe)
    {
        const auto* parent_pl =
            dynamic_cast<const playlist_impl*>(parent_base_maybe);
        if (!parent_pl)
            throw std::invalid_argument{
                "Parent playlist does not belong to this database"};
        if (parent_pl->id_ == id_)
            throw playlist_invalid_parent{
                "Cannot set playlist parent to itself"};
        new_parent_id = parent_pl->id_;
    }

    util::sqlite_transaction trans{context_->db};
    context_->db
        << "UPDATE playlist SET playlist_id_parent = ? "
           "WHERE playlist_id = ?;"
        << new_parent_id << id_;
    trans.commit();
}

std::optional<playlist> playlist_impl::sub_playlist_by_name(
    const std::string& name)
{
    int64_t found_id = 0;
    context_->db
        << "SELECT playlist_id FROM playlist "
           "WHERE playlist_id_parent = ? AND name = ?;"
        << id_ << name >>
        found_id;
    if (found_id == 0)
        return std::nullopt;

    return playlist{
        std::make_shared<playlist_impl>(context_, found_id)};
}

std::vector<track> playlist_impl::tracks() const
{
    playlist_content_table pct{context_};
    auto ids = pct.tracks_in(id_);

    std::vector<track> results;
    results.reserve(ids.size());
    for (auto&& track_id : ids)
    {
        results.emplace_back(
            std::make_shared<track_impl>(context_, track_id));
    }
    return results;
}

}  // namespace djinterop::onelibrary