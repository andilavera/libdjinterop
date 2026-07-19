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
#include <string>
#include <vector>

#include "pmai_writer.hpp"  // for tag_section

namespace djinterop::onelibrary::anlz
{

/// Read a big-endian uint32_t from a buffer at the given offset.
inline uint32_t read_u32_be(const uint8_t* data, size_t offset = 0)
{
    return (uint32_t{data[offset]} << 24) |
           (uint32_t{data[offset + 1]} << 16) |
           (uint32_t{data[offset + 2]} << 8) |
           uint32_t{data[offset + 3]};
}

/// Read a big-endian uint16_t from a buffer at the given offset.
inline uint16_t read_u16_be(const uint8_t* data, size_t offset = 0)
{
    return static_cast<uint16_t>(
        (data[offset] << 8) | data[offset + 1]);
}

/// Parse PMAI container data in memory and return the list of tags.
///
/// Reads the 28-byte PMAI header, then iterates tagged sections by
/// `len_tag` until the end of the data.  Each tag is returned as a
/// `tag_section` with its fourcc, metadata, and raw payload.
///
/// \param data Raw bytes of a PMAI (.DAT/.EXT/.2EX) file.
/// \return Ordered list of parsed tags.
std::vector<tag_section> parse_pmai(const std::vector<uint8_t>& data);

/// Parse a PMAI (ANLZ) file from disk.
///
/// \param path File path (e.g. `.PIONEER/USBANLZ/PXXX/XXXXXXXX/ANLZ0000.DAT`).
/// \return Ordered list of parsed tags.
std::vector<tag_section> parse_pmai_file(const std::string& path);

/// Find the first tag with the given fourcc, or nullptr if not found.
const tag_section* find_tag(
    const std::vector<tag_section>& tags, const std::string& fourcc);

/// Read the volume-relative path from a PPTH tag payload.
///
/// The payload is UTF-16BE text with a trailing NUL.  This function
/// decodes it to a UTF-8 string.
///
/// \param payload Raw PPTH payload bytes (from `tag_section::payload`).
/// \return Decoded path string.
std::string read_ppth(const std::vector<uint8_t>& payload);

}  // namespace djinterop::onelibrary::anlz
