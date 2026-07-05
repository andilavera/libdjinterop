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

namespace djinterop::onelibrary::anlz
{

/// A single tagged section within a PMAI container.
struct tag_section
{
    /// 4-character ASCII tag ID (e.g. "PPTH", "PCOB", "PQTZ").
    std::string id;

    /// Tag-specific metadata value (u32_meta in the 12-byte header).
    uint32_t meta;

    /// Raw payload bytes (not including the 12-byte header).
    std::vector<uint8_t> payload;

    /// PPTH has a 16-byte header (extra 4-byte path_len field).
    /// Set to true for PPTH tags.
    bool is_ppth = false;
};

/// Write a complete PMAI (ANLZ) file.
///
/// Produces the 28-byte PMAI header followed by the tagged sections.
/// All multi-byte integers are written in big-endian byte order.
void write_pmai_file(
    const std::string& path, const std::vector<tag_section>& tags);

/// Serialize a tag section to bytes (12- or 16-byte header + payload).
std::vector<uint8_t> serialize_tag(const tag_section& tag);

/// Write a big-endian uint32_t to a buffer at the given offset.
void write_u32_be(std::vector<uint8_t>& buf, size_t offset, uint32_t val);

/// Write a big-endian uint16_t to a buffer at the given offset.
void write_u16_be(std::vector<uint8_t>& buf, size_t offset, uint16_t val);

}  // namespace djinterop::onelibrary::anlz