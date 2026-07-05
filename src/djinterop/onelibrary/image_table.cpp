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

#include "image_table.hpp"

#include <sqlite_modern_cpp.h>

#include "onelibrary_context.hpp"

namespace djinterop::onelibrary
{

image_table::image_table(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t image_table::add(const std::string& path)
{
    int64_t next_id = 0;
    context_->db
        << "SELECT COALESCE(MAX(image_id), 0) + 1 FROM image;" >> next_id;

    context_->db
        << "INSERT INTO image (image_id, path) VALUES (?, ?);"
        << next_id << path;

    return next_id;
}

}  // namespace djinterop::onelibrary