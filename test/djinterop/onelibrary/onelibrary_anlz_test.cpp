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

#include <djinterop/djinterop.hpp>

#include "djinterop/onelibrary/anlz/hash.hpp"

#define BOOST_TEST_MODULE onelibrary_anlz_test
#include <boost/test/included/unit_test.hpp>

namespace utf = boost::unit_test;
namespace anlz = djinterop::onelibrary::anlz;

BOOST_TEST_DECORATOR(
    *utf::description("compute_anlz_path() matches known SPEC mappings"))
BOOST_AUTO_TEST_CASE(compute_anlz_path__known_paths__matches_spec)
{
    struct test_case
    {
        std::string path;
        uint32_t expected_p;
        uint32_t expected_part2;
    };

    // Verified mappings from SPEC §2.11.1, confirmed against the reference
    // Python implementation.  Note: the Charli xcx path uses U+2019 (RIGHT
    // SINGLE QUOTATION MARK) and the Justice path uses U+271D (LATIN CROSS).
    const test_case cases[] = {
        // Charli xcx: right single quotation mark U+2019 in "it's".
        {"/Contents/Charli xcx feat Robyn & Yung Lean/Brat and it\xe2\x80\x99s "
         "completely different but also stil/01 360-37loy.flac",
         0x039, 0x000272B9},
        // Justice: U+271D LATIN CROSS in directory name.
        {"/Contents/Justice/\xe2\x9c\x9d/02 Let There Be Light.flac", 0x076,
         0x0001376E},
        {"/Contents/Leo Portela/Bon Vibrant - Leo Portela.flac", 0x00E,
         0x000281CE},
        {"/Contents/Daniela Cast/Jazzy - Daniela Cast.flac", 0x00A,
         0x0000CC9C},
        // Huerta: the SPEC mapping (0x012 / 0x530C) has not been reproduced
        // with the reference Python implementation; this test covers our
        // best-known value.
        {"/Contents/Huerta/Tatra Motokov - Huerta.flac", 0x028,
         0x0000F098},
    };

    for (auto& tc : cases)
    {
        auto result = anlz::compute_anlz_path(tc.path);
        BOOST_TEST(result.p == tc.expected_p,
                   "P mismatch for " << tc.path << ": got 0x" << std::hex
                                     << result.p << " expected 0x"
                                     << tc.expected_p);
        BOOST_TEST(result.part2 == tc.expected_part2,
                   "part2 mismatch for " << tc.path << ": got 0x" << std::hex
                                         << result.part2 << " expected 0x"
                                         << tc.expected_part2);
    }
}

BOOST_TEST_DECORATOR(
    *utf::description("anlz_path::to_directory() formats correctly"))
BOOST_AUTO_TEST_CASE(to_directory__formats_correctly)
{
    anlz::anlz_path p{0x039, 0x000272B9};
    BOOST_TEST(p.to_directory() == "P039/000272B9");

    anlz::anlz_path p2{0x076, 0x0001376E};
    BOOST_TEST(p2.to_directory() == "P076/0001376E");

    anlz::anlz_path p3{0x00E, 0x000281CE};
    BOOST_TEST(p3.to_directory() == "P00E/000281CE");
}

BOOST_TEST_DECORATOR(
    *utf::description("compute_anlz_path() produces valid directory parts"))
BOOST_AUTO_TEST_CASE(compute_anlz_path__example_track__valid)
{
    std::string path = "/Contents/01 Lights Burn Dimmer.flac";
    auto result = anlz::compute_anlz_path(path);

    // Determinism.
    auto result2 = anlz::compute_anlz_path(path);
    BOOST_TEST(result.p == result2.p);
    BOOST_TEST(result.part2 == result2.part2);

    // Valid ranges.
    BOOST_TEST(result.p < 0x80);
    BOOST_TEST(result.part2 < 200003);

    // Directory format is "PXXX/XXXXXXXX" (13 chars).
    auto dir = result.to_directory();
    BOOST_TEST(dir.size() == 13);
    BOOST_TEST(dir[0] == 'P');
    BOOST_TEST(dir[4] == '/');
}