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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace djinterop::onelibrary
{
struct onelibrary_context;

struct playlist_row
{
    int64_t id = 0;
    int64_t sequence_no = 0;
    std::string name;
    int64_t image_id = 0;
    int64_t attribute = 0;       // 0 = playlist, 1 = folder
    int64_t parent_id = 0;       // 0 = root

    /// Construct from the flat column list returned by a full SELECT.
    /// Parameter order must match k_select_playlist columns.
    static playlist_row from_columns(
        int64_t playlist_id, int64_t sequence_no, std::string name,
        int64_t image_id, int64_t attribute, int64_t parent_id);
};

/// Exception thrown when a playlist row operation fails due to an invalid id.
struct playlist_row_id_error : public std::runtime_error
{
    explicit playlist_row_id_error(const std::string& what_arg) noexcept :
        runtime_error{what_arg} {}
};

class playlist_table
{
public:
    explicit playlist_table(std::shared_ptr<onelibrary_context> context);

    /// Add a playlist/folder row.  sequenceNo is auto-assigned.
    int64_t add(const playlist_row& row);

    /// Get a row by id.
    std::optional<playlist_row> get(int64_t id) const;

    /// Get all child ids of a parent (use 0 for root).
    std::vector<int64_t> children_of(int64_t parent_id) const;

    /// Check if a playlist exists.
    bool exists(int64_t id) const;

private:
    std::shared_ptr<onelibrary_context> context_;
};

class playlist_content_table
{
public:
    explicit playlist_content_table(
        std::shared_ptr<onelibrary_context> context);

    /// Add a track to a playlist.  sequenceNo is auto-assigned.
    void add(int64_t playlist_id, int64_t content_id);

    /// Get the ordered list of content_ids in a playlist.
    std::vector<int64_t> tracks_in(int64_t playlist_id) const;

    /// Remove a track from a playlist.
    void remove(int64_t playlist_id, int64_t content_id);

private:
    std::shared_ptr<onelibrary_context> context_;
};

}  // namespace djinterop::onelibrary