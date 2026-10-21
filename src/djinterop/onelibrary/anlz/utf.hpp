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

#include <cstddef>
#include <cstdint>
#include <string>

#include "format.hpp"

namespace djinterop::onelibrary::anlz::utf
{

/// Decode a single UTF-8 code point, advancing `it`.
inline uint32_t next_code_point(const char*& it, const char* end)
{
    if (it >= end)
        return 0;

    const auto c = static_cast<unsigned char>(*it++);
    if (c < 0x80)
        return c;
    if (c < 0xC0)
        return 0;

    auto trail = [&it, end]() {
        return it < end ? static_cast<uint32_t>(
                              static_cast<unsigned char>(*it++) & 0x3F)
                        : 0;
    };

    if (c < 0xE0)
        return ((c & 0x1Fu) << 6) | trail();
    if (c < 0xF0)
        return ((c & 0x0Fu) << 12) | (trail() << 6) | trail();
    return ((c & 0x07u) << 18) | (trail() << 12) | (trail() << 6) | trail();
}

/// Convert a UTF-8 string to UTF-16, including surrogate pairs.
inline std::u16string to_utf16(const std::string& utf8)
{
    std::u16string result;
    result.reserve(utf8.size());

    const char* it = utf8.data();
    const char* end = it + utf8.size();
    while (it < end)
    {
        const uint32_t cp = next_code_point(it, end);
        if (cp < 0x10000)
        {
            result.push_back(static_cast<char16_t>(cp));
        }
        else
        {
            const uint32_t offset = cp - 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (offset >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (offset & 0x3FF)));
        }
    }
    return result;
}

/// Convert UTF-16BE bytes, up to any NUL, into a UTF-8 string.
inline std::string from_utf16be(const uint8_t* data, size_t len)
{
    std::string result;

    for (size_t i = 0; i + 1 < len; i += 2)
    {
        uint32_t cp = read_u16_be(data, i);
        if (cp == 0)
            break;

        if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < len)
        {
            const uint32_t trail = read_u16_be(data, i + 2);
            if (trail >= 0xDC00 && trail <= 0xDFFF)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (trail - 0xDC00);
                i += 2;
            }
        }

        if (cp < 0x80)
        {
            result.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    return result;
}

}  // namespace djinterop::onelibrary::anlz::utf
