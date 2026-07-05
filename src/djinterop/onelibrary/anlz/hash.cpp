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

#include "hash.hpp"

#include <array>
#include <cstdio>

namespace djinterop::onelibrary::anlz
{

namespace
{
// Decode a single UTF-8 code point, advancing `it`.  Returns 0 on error/end.
uint32_t next_code_point(const char*& it, const char* end)
{
    if (it >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*it);

    if (c < 0x80) { ++it; return c; }
    if (c < 0xC0) { ++it; return 0; }       // continuation byte – skip
    if (c < 0xE0) {                          // 2-byte
        if (it + 1 >= end) { ++it; return 0; }
        uint32_t cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(it[1]) & 0x3Fu);
        it += 2; return cp;
    }
    if (c < 0xF0) {                          // 3-byte
        if (it + 2 >= end) { ++it; return 0; }
        uint32_t cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(it[1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(it[2]) & 0x3Fu);
        it += 3; return cp;
    }
    // 4-byte
    if (it + 3 >= end) { ++it; return 0; }
    uint32_t cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(it[1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(it[2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(it[3]) & 0x3Fu);
    it += 4; return cp;
}
}  // anonymous namespace

anlz_path compute_anlz_path(const std::string& file_path)
{
    // SPEC §2.11.1: iterates Unicode code points, matches Python `ord(ch)`.
    uint32_t h = 0;
    const char* it = file_path.data();
    const char* end = it + file_path.size();

    while (it < end)
    {
        uint32_t c = next_code_point(it, end) & 0xFFFF;
        h = ((h * 0x5BC9 + c) & 0xFFFFFFFF);
        h = ((h * 0x93B5 + c) & 0xFFFFFFFF);
    }

    uint32_t part2 = h % 200003;

    // 7 non-contiguous bits of part2.
    uint32_t p = ((part2 >> 0) & 0x01) | ((part2 >> 1) & 0x02) |
                 ((part2 >> 4) & 0x04) | ((part2 >> 4) & 0x08) |
                 ((part2 >> 5) & 0x10) | ((part2 >> 8) & 0x20) |
                 ((part2 >> 10) & 0x40);

    return {p, part2};
}

std::string anlz_path::to_directory() const
{
    std::array<char, 16> buf{};
    std::snprintf(buf.data(), buf.size(), "P%03X/%08X", p, part2);
    return {buf.data()};
}

}  // namespace djinterop::onelibrary::anlz