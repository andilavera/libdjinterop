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

#define BOOST_TEST_MODULE onelibrary_test
#include <boost/test/included/unit_test.hpp>

#include <djinterop/djinterop.hpp>

#include "../temporary_directory.hpp"

namespace utf = boost::unit_test;
namespace onelib = djinterop::onelibrary;

BOOST_TEST_DECORATOR(*utf::description("exists() returns false for empty dir"))
BOOST_AUTO_TEST_CASE(exists__empty_dir__returns_false)
{
    temporary_directory tmp_loc;
    BOOST_TEST(!onelib::onelibrary::exists(tmp_loc.temp_dir));
}

BOOST_TEST_DECORATOR(
    *utf::description("load() throws if database does not exist"))
BOOST_AUTO_TEST_CASE(load__no_database__throws)
{
    temporary_directory tmp_loc;
    BOOST_CHECK_THROW(
        onelib::onelibrary::load(tmp_loc.temp_dir), std::runtime_error);
}
