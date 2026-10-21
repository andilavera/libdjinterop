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
#include <vector>

namespace djinterop::onelibrary::anlz
{

/// Size of the PMAI file header in bytes.
constexpr size_t pmai_header_size = 28;

/// Size of the shortest tagged section header in bytes.
constexpr size_t min_tag_header_size = 12;

inline uint16_t read_u16_be(const uint8_t* data, size_t offset = 0)
{
    return static_cast<uint16_t>(
        (data[offset] << 8) | data[offset + 1]);
}

inline uint32_t read_u32_be(const uint8_t* data, size_t offset = 0)
{
    return (uint32_t{data[offset]} << 24) | (uint32_t{data[offset + 1]} << 16) |
           (uint32_t{data[offset + 2]} << 8) | uint32_t{data[offset + 3]};
}

inline void write_u16_be(
    std::vector<uint8_t>& buf, size_t offset, uint16_t val)
{
    buf[offset] = static_cast<uint8_t>(val >> 8);
    buf[offset + 1] = static_cast<uint8_t>(val);
}

inline void write_u32_be(
    std::vector<uint8_t>& buf, size_t offset, uint32_t val)
{
    buf[offset] = static_cast<uint8_t>(val >> 24);
    buf[offset + 1] = static_cast<uint8_t>(val >> 16);
    buf[offset + 2] = static_cast<uint8_t>(val >> 8);
    buf[offset + 3] = static_cast<uint8_t>(val);
}

/// A single tagged section within a PMAI container.
struct tag_section
{
    /// 4-character ASCII tag ID (e.g. "PPTH", "PCOB", "PQTZ").
    std::string id;

    /// Declared header length, echoed back from the file.
    uint32_t len_header = 0;

    /// Raw payload bytes, not including the section header.
    std::vector<uint8_t> payload;
};

/// Bytes preceding the payload; PPTH alone carries the path length.
inline size_t tag_header_size(const std::string& id)
{
    return id == "PPTH" ? 16 : min_tag_header_size;
}

}  // namespace djinterop::onelibrary::anlz
