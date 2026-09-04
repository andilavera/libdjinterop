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
#ifndef DJINTEROP_ONELIBRARY_ONELIBRARY_HPP
#define DJINTEROP_ONELIBRARY_ONELIBRARY_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <djinterop/config.hpp>

#include <djinterop/onelibrary/artist_table.hpp>
#include <djinterop/onelibrary/album_table.hpp>

namespace djinterop::onelibrary
{
struct onelibrary_context;
class artist_table;
class album_table;

/// Known static passphrase for Device Library Plus databases.
constexpr const char* default_key =
    "r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls";

/// Device Library Plus schema version.
constexpr const char* db_version = "1000";

/// Represents a Device Library Plus (OneLibrary) export.
///
/// Analogous to `engine::v3::engine_library`.
class DJINTEROP_PUBLIC onelibrary
{
public:
    /// Create a new, empty OneLibrary export at the given volume root.
    static onelibrary create(const std::string& directory);

    /// Load an existing OneLibrary export from the given volume root.
    static onelibrary load(const std::string& directory);

    /// Test whether a OneLibrary database exists at the given volume root.
    static bool exists(const std::string& directory);

    /// Destructor.
    ~onelibrary();

    /// Get the volume root directory.
    std::string directory() const;

    /// Get the number of tracks in the library.
    int64_t track_count() const;

    /// Verify the database integrity.
    void verify() const;

    /// Gets a class representing the artist table.
    artist_table artist() const noexcept { return artist_table{context_}; }

    /// Gets a class representing the album table.
    album_table album() const noexcept { return album_table{context_}; }

private:
    explicit onelibrary(std::shared_ptr<onelibrary_context> context);

    std::shared_ptr<onelibrary_context> context_;
    std::unique_ptr<artist_table> artist_;
    std::unique_ptr<album_table> album_;
};

}  // namespace djinterop::onelibrary

#endif  // DJINTEROP_ONELIBRARY_ONELIBRARY_HPP
