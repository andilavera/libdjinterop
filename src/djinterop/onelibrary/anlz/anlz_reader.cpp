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

#include "convert.hpp"

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

// =========================================================================
// PQTZ — beat grid reader
// =========================================================================

std::vector<beatgrid_marker> read_pqtz(
    const std::vector<uint8_t>& payload, double sample_rate)
{
    // Layout: pad(4) + 0x80000(4) + count(4) + entries(8 each)
    if (payload.size() < 12)
        return {};

    uint32_t count = read_u32_be(payload.data(), 8);
    if (payload.size() < 12 + static_cast<size_t>(count) * 8)
        return {};

    std::vector<beatgrid_marker> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 8;
        // beat_number (u16) and tempo (u16) at off and off+2.
        uint32_t time_ms = read_u32_be(payload.data(), off + 4);
        double sample_offset = convert::ms_to_sample_offset(
            time_ms, sample_rate);
        result.push_back(
            {static_cast<int32_t>(i), sample_offset});
    }
    return result;
}

// =========================================================================
// PCOB — cue / loop reader
// =========================================================================

pcob_cues read_pcob(
    const std::vector<uint8_t>& payload, double sample_rate)
{
    pcob_cues result;

    // Layout: type(u4) + unk(u2) + count(u2) + memory_count(u4) + entries
    if (payload.size() < 12)
        return result;

    uint32_t container_type = read_u32_be(payload.data(), 0);
    uint16_t count = read_u16_be(payload.data(), 6);

    size_t offset = 12;
    for (uint16_t i = 0; i < count; ++i)
    {
        if (offset + 12 > payload.size())
            break;

        // PCPT sub-tag header: "PCPT"(4) + len_header(4) + len_entry(4)
        uint32_t len_entry = read_u32_be(payload.data(), offset + 8);
        if (len_entry < 12 || offset + len_entry > payload.size())
            break;

        // Body starts at offset + 12.
        size_t b = offset + 12;
        if (b + 40 > payload.size())
            break;

        uint32_t hot_cue = read_u32_be(payload.data(), b);
        // status at b+4, u1 at b+8, order_first at b+12, order_last at b+14
        uint8_t entry_type = payload[b + 16];  // 1=point, 2=loop
        uint32_t time_ms = read_u32_be(payload.data(), b + 20);
        uint32_t loop_time_ms = read_u32_be(payload.data(), b + 24);

        double sample_offset =
            convert::ms_to_sample_offset(time_ms, sample_rate);

        if (container_type == 1)
        {
            // Hot cues: hot_cue is 1-based slot index.
            if (hot_cue >= 1 && hot_cue <= 8)
            {
                auto idx = static_cast<size_t>(hot_cue - 1);
                if (idx >= result.hot_cues.size())
                    result.hot_cues.resize(idx + 1);
                djinterop::hot_cue hc{};
                hc.sample_offset = sample_offset;
                result.hot_cues[idx] = hc;
            }
        }
        else if (entry_type == 2)
        {
            // Memory loop: time_ms is start, loop_time_ms is duration.
            djinterop::loop lp{};
            lp.start_sample_offset = sample_offset;
            if (loop_time_ms != 0xFFFFFFFF)
                lp.end_sample_offset =
                    convert::ms_to_sample_offset(
                        time_ms + loop_time_ms, sample_rate);
            else
                lp.end_sample_offset = sample_offset;
            result.loops.push_back(lp);
        }

        offset += len_entry;
    }

    return result;
}

// =========================================================================
// PCO2 — extended cue / loop reader
// =========================================================================

namespace
{
std::string decode_utf16be(const uint8_t* data, size_t len)
{
    std::string result;
    for (size_t i = 0; i + 1 < len; i += 2)
    {
        uint16_t unit = read_u16_be(data, i);
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < len)
        {
            uint16_t lo = read_u16_be(data, i + 2);
            if (lo >= 0xDC00 && lo <= 0xDFFF)
            {
                uint32_t cp = 0x10000 + ((unit - 0xD800) << 10) +
                              (lo - 0xDC00);
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
}  // anonymous namespace

pcob_cues read_pco2(
    const std::vector<uint8_t>& payload, double sample_rate)
{
    pcob_cues result;

    // Layout: type(u4) + count(u2) + unknown(u2) + entries
    if (payload.size() < 8)
        return result;

    uint32_t container_type = read_u32_be(payload.data(), 0);
    uint16_t count = read_u16_be(payload.data(), 4);

    size_t offset = 8;
    for (uint16_t i = 0; i < count; ++i)
    {
        if (offset + 12 > payload.size())
            break;

        // PCP2 sub-tag header: "PCP2"(4) + len_header(4) + len_entry(4)
        uint32_t len_entry = read_u32_be(payload.data(), offset + 8);
        if (len_entry < 12 || offset + len_entry > payload.size())
            break;

        size_t b = offset + 12;
        if (b + 32 > payload.size())
            break;

        uint32_t hot_cue = read_u32_be(payload.data(), b);
        uint8_t entry_type = payload[b + 4];
        uint32_t time_ms = read_u32_be(payload.data(), b + 8);
        uint32_t loop_time_ms = read_u32_be(payload.data(), b + 12);
        // color_id at b+16, pad(7) at b+17..b+23
        // loop_enum at b+24, loop_denom at b+26
        uint32_t len_comment = read_u32_be(payload.data(), b + 28);

        // Comment (UTF-16BE) starts at b+32.
        size_t comment_off = b + 32;
        if (comment_off + len_comment > payload.size())
            break;

        std::string label = decode_utf16be(
            payload.data() + comment_off, len_comment);

        // Color fields follow the comment.
        size_t cb = comment_off + len_comment;
        if (cb + 4 > payload.size())
            break;
        // color_code at cb, r at cb+1, g at cb+2, b at cb+3
        uint8_t r = payload[cb + 1];
        uint8_t g = payload[cb + 2];
        uint8_t blu = payload[cb + 3];
        pad_color color{r, g, blu, 255};

        double sample_offset =
            convert::ms_to_sample_offset(time_ms, sample_rate);

        if (container_type == 1)
        {
            // Hot cues: hot_cue is 1-based slot index.
            if (hot_cue >= 1 && hot_cue <= 8)
            {
                auto idx = static_cast<size_t>(hot_cue - 1);
                if (idx >= result.hot_cues.size())
                    result.hot_cues.resize(idx + 1);
                djinterop::hot_cue hc{};
                hc.label = label;
                hc.sample_offset = sample_offset;
                hc.color = color;
                result.hot_cues[idx] = hc;
            }
        }
        else if (entry_type == 2)
        {
            // Memory loop: time_ms is start, loop_time_ms is duration.
            djinterop::loop lp{};
            lp.label = label;
            lp.start_sample_offset = sample_offset;
            if (loop_time_ms != 0xFFFFFFFF)
                lp.end_sample_offset =
                    convert::ms_to_sample_offset(
                        time_ms + loop_time_ms, sample_rate);
            else
                lp.end_sample_offset = sample_offset;
            lp.color = color;
            result.loops.push_back(lp);
        }

        offset += len_entry;
    }

    return result;
}

// =========================================================================
// Waveform readers
// =========================================================================

std::vector<waveform_entry> read_pwav(const std::vector<uint8_t>& payload)
{
    // Header: count(u4) + 0x00010000(u4) + data.
    if (payload.size() < 8)
        return {};

    uint32_t count = read_u32_be(payload.data(), 0);
    if (payload.size() < 8 + count)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        result.push_back(convert::from_pwav_byte(payload[8 + i]));
    return result;
}

std::vector<waveform_entry> read_pwv2(const std::vector<uint8_t>& payload)
{
    // Header: count(u4) + 0x00010000(u4) + data.
    if (payload.size() < 8)
        return {};

    uint32_t count = read_u32_be(payload.data(), 0);
    if (payload.size() < 8 + count)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        result.push_back(convert::from_pwv2_byte(payload[8 + i]));
    return result;
}

std::vector<waveform_entry> read_pwv3(const std::vector<uint8_t>& payload)
{
    // Header: 1(u4) + count(u4) + 0x00960000(u4) + data.
    if (payload.size() < 12)
        return {};

    uint32_t count = read_u32_be(payload.data(), 4);
    if (payload.size() < 12 + count)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        result.push_back(convert::from_pwv3_byte(payload[12 + i]));
    return result;
}

std::vector<waveform_entry> read_pwv4(const std::vector<uint8_t>& payload)
{
    // Header: 6(u4) + count(u4) + unknown(u4) + data (6 bytes/entry).
    if (payload.size() < 12)
        return {};

    uint32_t count = read_u32_be(payload.data(), 4);
    if (payload.size() < 12 + static_cast<size_t>(count) * 6)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 6;
        result.push_back(convert::from_pwv4_entry(payload.data() + off));
    }
    return result;
}

std::vector<waveform_entry> read_pwv5(const std::vector<uint8_t>& payload)
{
    // Header: 2(u4) + count(u4) + unknown(u4) + data (2 bytes/entry, BE).
    if (payload.size() < 12)
        return {};

    uint32_t count = read_u32_be(payload.data(), 4);
    if (payload.size() < 12 + static_cast<size_t>(count) * 2)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 2;
        uint16_t v = read_u16_be(payload.data(), off);
        result.push_back(convert::from_pwv5_entry(v));
    }
    return result;
}

std::vector<waveform_entry> read_pwv6(const std::vector<uint8_t>& payload)
{
    // Header: 3(u4) + count(u4) + data (3 bytes/entry: mid, high, low).
    if (payload.size() < 8)
        return {};

    uint32_t count = read_u32_be(payload.data(), 4);
    if (payload.size() < 8 + static_cast<size_t>(count) * 3)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 8 + i * 3;
        result.push_back(convert::from_pwv67_entry(
            payload[off], payload[off + 1], payload[off + 2]));
    }
    return result;
}

std::vector<waveform_entry> read_pwv7(const std::vector<uint8_t>& payload)
{
    // Header: 3(u4) + count(u4) + 0x00960000(u4) + data (3 bytes/entry).
    if (payload.size() < 12)
        return {};

    uint32_t count = read_u32_be(payload.data(), 4);
    if (payload.size() < 12 + static_cast<size_t>(count) * 3)
        return {};

    std::vector<waveform_entry> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 3;
        result.push_back(convert::from_pwv67_entry(
            payload[off], payload[off + 1], payload[off + 2]));
    }
    return result;
}

}  // namespace djinterop::onelibrary::anlz
