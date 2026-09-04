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

#include <djinterop/onelibrary/reference_tables.hpp>

#include <sqlite_modern_cpp.h>

#include "onelibrary_context.hpp"

namespace djinterop::onelibrary
{

namespace
{
int64_t add_simple(
    sqlite::database& db, const char* table_name, const char* id_col,
    const std::string& name)
{
    // Check if already exists.
    std::string find_sql = std::string{"SELECT COALESCE("
                                       "(SELECT "} +
                           id_col + " FROM " + table_name +
                           " WHERE name = ?), 0);";
    int64_t existing = 0;
    db << find_sql << name >> existing;
    if (existing != 0) return existing;

    // Compute next id and insert.
    std::string max_sql =
        std::string{"SELECT COALESCE(MAX("} + id_col + "), 0) + 1 FROM " +
        table_name + ";";
    int64_t next_id = 0;
    db << max_sql >> next_id;

    std::string insert_sql = std::string{"INSERT INTO "} + table_name + " (" +
                             id_col + ", name) VALUES (?, ?);";
    db << insert_sql << next_id << name;

    return next_id;
}

std::optional<int64_t> find_id_simple(
    sqlite::database& db, const char* table_name, const char* id_col,
    const std::string& name)
{
    std::string sql = std::string{"SELECT COALESCE("
                                  "(SELECT "} +
                      id_col + " FROM " + table_name +
                      " WHERE name = ?), 0);";
    int64_t id = 0;
    db << sql << name >> id;
    if (id == 0)
        return std::nullopt;
    return id;
}
}  // anonymous namespace

genre_table::genre_table(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t genre_table::add(const std::string& name)
{
    return add_simple(context_->db, "genre", "genre_id", name);
}

std::optional<int64_t> genre_table::find_id(const std::string& name) const
{
    return find_id_simple(context_->db, "genre", "genre_id", name);
}

label_table::label_table(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t label_table::add(const std::string& name)
{
    return add_simple(context_->db, "label", "label_id", name);
}

std::optional<int64_t> label_table::find_id(const std::string& name) const
{
    return find_id_simple(context_->db, "label", "label_id", name);
}

key_table::key_table(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t key_table::add(const std::string& name)
{
    return add_simple(context_->db, "key", "key_id", name);
}

std::optional<int64_t> key_table::find_id(const std::string& name) const
{
    return find_id_simple(context_->db, "key", "key_id", name);
}

}  // namespace djinterop::onelibrary