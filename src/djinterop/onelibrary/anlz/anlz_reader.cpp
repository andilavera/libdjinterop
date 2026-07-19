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

#include "anlz_reader.hpp"

#include <fstream>
#include <stdexcept>

namespace djinterop::onelibrary::anlz
{

namespace
{
constexpr size_t k_pmai_header_size = 28;

bool is_ppth(const std::string& fourcc)
{
    return fourcc == "PPTH";
}
}  // anonymous namespace

std::vector<tag_section> parse_pmai(const std::vector<uint8_t>& data)
{
    if (data.size() < k_pmai_header_size)
        throw std::runtime_error{
            "ANLZ data too short for PMAI header"};

    // Verify PMAI magic.
    if (data[0] != 'P' || data[1] != 'M' ||
        data[2] != 'A' || data[3] != 'I')
    {
        throw std::runtime_error{"Invalid PMAI magic"};
    }

    uint32_t len_file = read_u32_be(data.data(), 8);
    if (len_file > data.size())
        throw std::runtime_error{"PMAI len_file exceeds data size"};

    std::vector<tag_section> tags;
    size_t offset = k_pmai_header_size;
    while (offset + 12 <= len_file)
    {
        // Tag header: fourcc(4) + len_header(4) + len_tag(4)
        std::string fourcc{reinterpret_cast<const char*>(&data[offset]), 4};
        uint32_t meta = read_u32_be(data.data(), offset + 4);
        uint32_t len_tag = read_u32_be(data.data(), offset + 8);

        if (len_tag < 12 || offset + len_tag > len_file)
            break;

        bool ppth = is_ppth(fourcc);
        size_t header_size = ppth ? 16 : 12;
        size_t payload_size = len_tag - header_size;

        tag_section tag;
        tag.id = fourcc;
        tag.meta = meta;
        tag.is_ppth = ppth;
        tag.payload.assign(
            data.begin() + offset + header_size,
            data.begin() + offset + header_size + payload_size);

        tags.push_back(std::move(tag));
        offset += len_tag;
    }

    return tags;
}

std::vector<tag_section> parse_pmai_file(const std::string& path)
{
    std::ifstream in{path, std::ios::binary | std::ios::ate};
    if (!in)
        throw std::runtime_error{"Cannot open ANLZ file: " + path};

    auto size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);

    return parse_pmai(data);
}

const tag_section* find_tag(
    const std::vector<tag_section>& tags, const std::string& fourcc)
{
    for (const auto& tag : tags)
        if (tag.id == fourcc)
            return &tag;
    return nullptr;
}

std::string read_ppth(const std::vector<uint8_t>& payload)
{
    // Payload is UTF-16BE, possibly with a trailing NUL (2 bytes).
    size_t len = payload.size();
    // Strip trailing NUL pair if present.
    if (len >= 2 && payload[len - 2] == 0 && payload[len - 1] == 0)
        len -= 2;

    std::string result;
    for (size_t i = 0; i + 1 < len; i += 2)
    {
        uint16_t unit = read_u16_be(payload.data(), i);
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < len)
        {
            // High surrogate; read low surrogate.
            uint16_t lo = read_u16_be(payload.data(), i + 2);
            if (lo >= 0xDC00 && lo <= 0xDFFF)
            {
                uint32_t cp = 0x10000 + ((unit - 0xD800) << 10) +
                              (lo - 0xDC00);
                // Encode as 4-byte UTF-8.
                result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                result.push_back(static_cast<char>(
                    0x80 | ((cp >> 12) & 0x3F)));
                result.push_back(static_cast<char>(
                    0x80 | ((cp >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                i += 2;
                continue;
            }
        }
        if (unit < 0x80)
        {
            result.push_back(static_cast<char>(unit));
        }
        else if (unit < 0x800)
        {
            result.push_back(static_cast<char>(0xC0 | (unit >> 6)));
            result.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
        }
        else
        {
            result.push_back(static_cast<char>(0xE0 | (unit >> 12)));
            result.push_back(static_cast<char>(
                0x80 | ((unit >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
        }
    }
    return result;
}

}  // namespace djinterop::onelibrary::anlz
