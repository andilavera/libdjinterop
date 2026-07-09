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

#include <djinterop/onelibrary/onelibrary_factory.hpp>

#include <memory>

#include <djinterop/onelibrary/onelibrary.hpp>

#include "database_impl.hpp"

namespace djinterop::onelibrary
{

database create_database(const std::string& directory)
{
    auto lib = onelibrary::create(directory);
    auto ctx = lib.context_;
    return database{std::make_shared<database_impl>(ctx)};
}

database load_database(const std::string& directory)
{
    auto lib = onelibrary::load(directory);
    auto ctx = lib.context_;
    return database{std::make_shared<database_impl>(ctx)};
}

database create_or_load_database(
    const std::string& directory, bool& created)
{
    if (database_exists(directory))
    {
        created = false;
        return load_database(directory);
    }

    created = true;
    return create_database(directory);
}

bool database_exists(const std::string& directory)
{
    return onelibrary::exists(directory);
}

}  // namespace djinterop::onelibrary