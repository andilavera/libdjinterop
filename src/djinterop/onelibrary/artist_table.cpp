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

#include <djinterop/onelibrary/artist_table.hpp>

#include <sqlite_modern_cpp.h>

#include "onelibrary_context.hpp"

namespace djinterop::onelibrary
{

artist_table::artist_table(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t artist_table::add(const std::string& name)
{
    // Check if already exists (no UNIQUE constraint in OneLibrary schema).
    auto existing = find_id(name);
    if (existing) return *existing;

    int64_t next_id = 0;
    context_->db
        << "SELECT COALESCE(MAX(artist_id), 0) + 1 FROM artist;" >> next_id;

    context_->db
        << "INSERT INTO artist (artist_id, name) VALUES (?, ?);"
        << next_id << name;

    return next_id;
}

std::optional<int64_t> artist_table::find_id(const std::string& name) const
{
    int64_t id = 0;
    context_->db
        << "SELECT COALESCE("
           "(SELECT artist_id FROM artist WHERE name = ?), 0);"
        << name >> id;
    if (id == 0)
        return std::nullopt;
    return id;
}

}  // namespace djinterop::onelibrary