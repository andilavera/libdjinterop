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

#include <optional>
#include <string>

#include <djinterop/musical_key.hpp>

namespace djinterop::onelibrary
{

/// Map a rekordbox key name (e.g. "Am", "F#m") to a musical_key enum value.
///
/// Rekordbox stores keys by name in the `key` table using ASCII notation
/// (e.g. "F#m" rather than "F♯m").  The musical_key enum follows the
/// Camelot wheel order, which matches the rekordbox key_id ordering.
///
/// Returns std::nullopt if the name is not recognised.
std::optional<musical_key> key_name_to_enum(const std::string& name);

/// Map a musical_key enum value to its rekordbox ASCII name.
///
/// Returns nullptr if the key value is not recognised.
const char* musical_key_to_name(musical_key k);

}  // namespace djinterop::onelibrary