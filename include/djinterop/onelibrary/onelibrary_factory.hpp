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
#ifndef DJINTEROP_ONELIBRARY_ONELIBRARY_FACTORY_HPP
#define DJINTEROP_ONELIBRARY_ONELIBRARY_FACTORY_HPP

#include <string>

#include <djinterop/config.hpp>
#include <djinterop/database.hpp>

namespace djinterop::onelibrary
{

/// Create a new, empty OneLibrary database in the given directory.
///
/// The directory should be the volume root (e.g. `/mnt/usb`).
/// The `.PIONEER/rekordbox/exportLibrary.db` path is created
/// automatically.
///
/// \param directory Volume root directory.
/// \return A high-level database handle.
database DJINTEROP_PUBLIC create_database(const std::string& directory);

/// Load an existing OneLibrary database from the given directory.
///
/// \param directory Volume root directory.
/// \return A high-level database handle.
database DJINTEROP_PUBLIC load_database(const std::string& directory);

/// Create or load a OneLibrary database in the given directory.
///
/// If a database already exists, it will be loaded. Otherwise, a new
/// empty database will be created.
///
/// \param directory Volume root directory.
/// \param created Output parameter: true if a new database was created.
/// \return A high-level database handle.
database DJINTEROP_PUBLIC create_or_load_database(
    const std::string& directory, bool& created);

/// Test whether a OneLibrary database exists in the given directory.
///
/// \param directory Volume root directory.
/// \return true if an exportLibrary.db exists.
bool DJINTEROP_PUBLIC database_exists(const std::string& directory);

}  // namespace djinterop::onelibrary

#endif  // DJINTEROP_ONELIBRARY_ONELIBRARY_FACTORY_HPP