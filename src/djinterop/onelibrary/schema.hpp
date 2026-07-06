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

#include <stdexcept>
#include <string>

namespace djinterop::onelibrary
{
/// Base exception for OneLibrary (Device Library Plus) errors.
struct onelibrary_error : public std::runtime_error
{
    explicit onelibrary_error(const std::string& what_arg) noexcept :
        runtime_error{what_arg} {}
};

/// Device Library Plus schema version as stored in property.dbVersion.
extern const char* const schema_db_version;

/// Create all 22 tables in the Device Library Plus schema.
///
/// Must be called on an already-opened, key-set database.  Does NOT insert
/// any data — use `insert_default_catalogues()` for that.
void create_tables(void* db_handle);

/// Insert the default catalogue rows required for a valid empty export.
///
/// Seeds the `color`, `menuItem`, `category`, `sort`, and `property` tables
/// with the fixed default rows (djay Pro defaults for producer variance).
void insert_default_catalogues(void* db_handle);

/// SQL statements that together define the canonical Device Library Plus schema
/// and default catalogue, as specified in the rekordbox USB Export Format
/// Specification §2.6–§2.9.

extern const char* ddl_content;
extern const char* ddl_artist;
extern const char* ddl_album;
extern const char* ddl_genre;
extern const char* ddl_label;
extern const char* ddl_key;
extern const char* ddl_color;
extern const char* ddl_image;
extern const char* ddl_cue;
extern const char* ddl_playlist;
extern const char* ddl_playlist_content;
extern const char* ddl_history;
extern const char* ddl_history_content;
extern const char* ddl_hotCueBankList;
extern const char* ddl_hotCueBankList_cue;
extern const char* ddl_myTag;
extern const char* ddl_myTag_content;
extern const char* ddl_menuItem;
extern const char* ddl_category;
extern const char* ddl_sort;
extern const char* ddl_property;
extern const char* ddl_recommendedLike;

extern const char* idx_playlist_content_playlist_id;
extern const char* idx_myTag_content_content_id;
extern const char* idx_myTag_content_myTag_id;
extern const char* idx_hotCueBankList_cue_hotCueBankList_id;

}  // namespace djinterop::onelibrary