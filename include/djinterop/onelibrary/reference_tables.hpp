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

#include <djinterop/config.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace djinterop::onelibrary
{
struct onelibrary_context;

class DJINTEROP_PUBLIC genre_table
{
public:
    explicit genre_table(std::shared_ptr<onelibrary_context> context);

    int64_t add(const std::string& name);
    std::optional<int64_t> find_id(
        const std::string& name) const;

private:
    std::shared_ptr<onelibrary_context> context_;
};

class DJINTEROP_PUBLIC label_table
{
public:
    explicit label_table(std::shared_ptr<onelibrary_context> context);

    int64_t add(const std::string& name);
    std::optional<int64_t> find_id(
        const std::string& name) const;

private:
    std::shared_ptr<onelibrary_context> context_;
};

class DJINTEROP_PUBLIC key_table
{
public:
    explicit key_table(std::shared_ptr<onelibrary_context> context);

    int64_t add(const std::string& name);
    std::optional<int64_t> find_id(
        const std::string& name) const;

private:
    std::shared_ptr<onelibrary_context> context_;
};

}  // namespace djinterop::onelibrary