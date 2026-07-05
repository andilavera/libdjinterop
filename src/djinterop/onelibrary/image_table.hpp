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
#include <string>

namespace djinterop::onelibrary
{
struct onelibrary_context;

class image_table
{
public:
    explicit image_table(std::shared_ptr<onelibrary_context> context);

    /// Add an image row and return its id.
    /// \param path Volume-relative path, e.g. /.PIONEER/Artwork/00001/b1.jpg
    int64_t add(const std::string& path);

private:
    std::shared_ptr<onelibrary_context> context_;
};

}  // namespace djinterop::onelibrary