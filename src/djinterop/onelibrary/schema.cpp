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

#include "schema.hpp"

#include <sqlite3.h>

#include <string>

namespace djinterop::onelibrary
{

namespace
{
int count_rows(sqlite3* db, const char* sql)
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        throw onelibrary_error{
            std::string{"SQL prepare error: "} + sqlite3_errmsg(db)};
    }
    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

}  // anonymous namespace

void verify(void* db_handle)
{
    auto* db = static_cast<sqlite3*>(db_handle);

    int tables =
        count_rows(db, "SELECT count(*) FROM sqlite_master WHERE type='table'");
    if (tables != 22)
    {
        throw onelibrary_error{
            "Expected 22 tables, found " + std::to_string(tables)};
    }

    int indexes =
        count_rows(db, "SELECT count(*) FROM sqlite_master WHERE type='index'");
    if (indexes != 4)
    {
        throw onelibrary_error{
            "Expected 4 indexes, found " + std::to_string(indexes)};
    }
}

}  // namespace djinterop::onelibrary
