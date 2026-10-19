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

// `PMAI` is the file-level fourcc header shared by all ANLZ files (`.DAT`, `.EXT`, `.2EX`).

#include "pmai_writer.hpp"

#include <fstream>
#include <stdexcept>

namespace djinterop::onelibrary::anlz
{

std::vector<uint8_t> serialize_tag(const tag_section& tag)
{
    const size_t header_size = tag_header_size(tag.id);
    const size_t total_size = header_size + tag.payload.size();
    std::vector<uint8_t> buf(total_size);

    // Tag ID (4 ASCII bytes).
    for (size_t i = 0; i < 4 && i < tag.id.size(); ++i)
        buf[i] = static_cast<uint8_t>(tag.id[i]);

    // len_header at offset 4.
    write_u32_be(buf, 4, tag.len_header);

    // len_tag_total at offset 8.
    write_u32_be(buf, 8, static_cast<uint32_t>(total_size));

    // PPTH extra: path_len at offset 12.
    if (header_size > min_tag_header_size)
        write_u32_be(buf, 12, static_cast<uint32_t>(tag.payload.size()));

    // Payload.
    for (size_t i = 0; i < tag.payload.size(); ++i)
        buf[header_size + i] = tag.payload[i];

    return buf;
}

void write_pmai_file(
    const std::string& path, const std::vector<tag_section>& tags)
{
    // Serialize all tags first to compute total file size.
    std::vector<std::vector<uint8_t>> serialized;
    serialized.reserve(tags.size());
    uint32_t total_size = pmai_header_size;
    for (auto& tag : tags)
    {
        serialized.push_back(serialize_tag(tag));
        total_size += static_cast<uint32_t>(serialized.back().size());
    }

    // Build the PMAI header.
    std::vector<uint8_t> header(pmai_header_size, 0);
    header[0] = 'P';
    header[1] = 'M';
    header[2] = 'A';
    header[3] = 'I';
    write_u32_be(header, 4, pmai_header_size);
    write_u32_be(header, 8, total_size);
    // Four words: 0x01, 0x10000, 0x10000, 0 (rekordbox convention)
    write_u32_be(header, 12, 0x01);
    write_u32_be(header, 16, 0x10000);
    write_u32_be(header, 20, 0x10000);
    write_u32_be(header, 24, 0x00);

    // Write to file.
    std::ofstream out{path, std::ios::binary};
    if (!out)
        throw std::runtime_error{"Cannot open ANLZ file for writing: " + path};

    out.write(reinterpret_cast<const char*>(header.data()), header.size());
    for (auto& s : serialized)
        out.write(reinterpret_cast<const char*>(s.data()), s.size());
}

}  // namespace djinterop::onelibrary::anlz
