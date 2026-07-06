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

#include <memory>
#include <string>

#include <sqlite_modern_cpp.h>

namespace djinterop::onelibrary
{
/// Internal context holding the SQLCipher database connection and directory
/// for a Device Library Plus (OneLibrary) export.
///
/// Analogous to `engine::engine_library_context`.
struct onelibrary_context
{
    onelibrary_context(std::string directory, sqlite::database db) :
        directory{std::move(directory)}, db{std::move(db)}
    {
    }

    /// The volume root directory (contains `.PIONEER/`).
    const std::string directory;

    /// The SQLCipher-encrypted SQLite database handle.
    sqlite::database db;
};

}  // namespace djinterop::onelibrary