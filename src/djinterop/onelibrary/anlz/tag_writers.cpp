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

#include "tag_writers.hpp"

#include <algorithm>
#include <cmath>

#include "convert.hpp"
#include "pmai_writer.hpp"

namespace djinterop::onelibrary::anlz
{

namespace
{
// Decode UTF-8 code points (same as hash.cpp).
uint32_t next_code_point(const char*& it, const char* end)
{
    if (it >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*it);
    if (c < 0x80) { ++it; return c; }
    if (c < 0xC0) { ++it; return 0; }
    if (c < 0xE0) {
        if (it + 1 >= end) { ++it; return 0; }
        uint32_t cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(it[1]) & 0x3Fu);
        it += 2; return cp;
    }
    if (c < 0xF0) {
        if (it + 2 >= end) { ++it; return 0; }
        uint32_t cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(it[1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(it[2]) & 0x3Fu);
        it += 3; return cp;
    }
    if (it + 3 >= end) { ++it; return 0; }
    uint32_t cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(it[1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(it[2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(it[3]) & 0x3Fu);
    it += 4; return cp;
}

}  // anonymous namespace

// =========================================================================
// PPTH
// =========================================================================

std::vector<uint8_t> build_ppth_payload(const std::string& utf8_path)
{
    size_t num_chars = 0;
    const char* it = utf8_path.data();
    const char* end = it + utf8_path.size();
    while (it < end)
    {
        unsigned char c = static_cast<unsigned char>(*it);
        if (c < 0x80) ++it;
        else if (c < 0xC0) ++it;
        else if (c < 0xE0) it += 2;
        else if (c < 0xF0) it += 3;
        else it += 4;
        ++num_chars;
    }

    size_t path_len = (num_chars + 1) * 2;
    std::vector<uint8_t> payload(path_len, 0);

    it = utf8_path.data();
    end = utf8_path.data() + utf8_path.size();
    size_t out = 0;
    while (it < end && out + 1 < path_len)
    {
        uint32_t cp = next_code_point(it, end) & 0xFFFF;
        write_u16_be(payload, out, static_cast<uint16_t>(cp));
        out += 2;
    }

    return payload;
}

// =========================================================================
// PCOB
// =========================================================================

std::vector<uint8_t> build_empty_pcob_payload(uint32_t container_type)
{
    std::vector<uint8_t> payload(12, 0);
    write_u32_be(payload, 0, container_type);
    write_u32_be(payload, 4, 0);
    write_u32_be(payload, 8, 0xFFFFFFFF);
    return payload;
}

namespace
{
// Build a PCOB container from pre-converted entry data.
// container_type: 1 = hot cues, 0 = memory/loops.
std::vector<uint8_t> build_pcob_from_entries(
    uint32_t container_type,
    const std::vector<convert::pcob_entry_data>& entries)
{
    if (entries.empty())
    {
        std::vector<uint8_t> payload(12, 0);
        write_u32_be(payload, 0, container_type);
        write_u32_be(payload, 8, 0xFFFFFFFF);
        return payload;
    }

    constexpr size_t entry_size = 56;
    uint16_t count = static_cast<uint16_t>(entries.size());
    size_t payload_size = 12 + count * entry_size;
    std::vector<uint8_t> payload(payload_size, 0);

    write_u32_be(payload, 0, container_type);
    write_u16_be(payload, 6, count);

    for (uint16_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * entry_size;
        const auto& e = entries[i];
        bool is_first = (i == 0);
        bool is_last = (i == count - 1);

        // PCPT sub-tag header.
        payload[off] = 'P';
        payload[off + 1] = 'C';
        payload[off + 2] = 'P';
        payload[off + 3] = 'T';
        write_u32_be(payload, off + 4, 28);   // len_header
        write_u32_be(payload, off + 8, 56);   // len_entry

        // Body starts at off + 12.
        size_t b = off + 12;
        write_u32_be(payload, b, e.hot_cue_idx);
        write_u32_be(payload, b + 4, 4);      // status = enabled
        write_u32_be(payload, b + 8, 0x00010000);
        write_u16_be(payload, b + 12, is_first ? 0xFFFF : 0);
        write_u16_be(payload, b + 14, is_last ? 0xFFFF : 1);
        payload[b + 16] = e.is_loop ? 2 : 1;  // type: 1=point, 2=loop
        write_u16_be(payload, b + 18, 1000);
        write_u32_be(payload, b + 20, e.time_ms);
        write_u32_be(payload, b + 24, e.loop_time_ms);
    }

    // memory_count: equals count for memory cuetype.
    if (container_type == 0)
        write_u32_be(payload, 8, count);

    return payload;
}
}  // anonymous namespace

std::vector<uint8_t> build_pcob_payload(
    const std::vector<std::optional<hot_cue>>& cues, double sample_rate)
{
    std::vector<convert::pcob_entry_data> entries;
    for (size_t i = 0; i < cues.size(); ++i)
    {
        auto d = convert::to_pcob_hot_cue_data(cues[i], i, sample_rate);
        if (d) entries.push_back(*d);
    }
    return build_pcob_from_entries(1, entries);
}

std::vector<uint8_t> build_pcob_payload(
    const std::vector<std::optional<loop>>& loops, double sample_rate)
{
    std::vector<convert::pcob_entry_data> entries;
    for (const auto& lp : loops)
    {
        auto d = convert::to_pcob_loop_data(lp, sample_rate);
        if (d) entries.push_back(*d);
    }
    return build_pcob_from_entries(0, entries);
}

// =========================================================================
// PQTZ — beat grid
//   pad(4) + 0x00080000(4) + count(4) + count × 8-byte entries
//   entry: beat_number(u16) + tempo_bpmx100(u16) + time_ms(u32)
//   beat_number cycles 1–4.
// =========================================================================

std::vector<uint8_t> build_pqtz_payload(
    const std::vector<beatgrid_marker>& beatgrid, double sample_rate)
{
    auto entries = convert::to_pqtz_entries(beatgrid, sample_rate);
    uint32_t entry_count = static_cast<uint32_t>(entries.size());

    size_t total = 12 + entry_count * 8;
    std::vector<uint8_t> payload(total, 0);

    write_u32_be(payload, 4, 0x00080000);
    write_u32_be(payload, 8, entry_count);

    for (uint32_t i = 0; i < entry_count; ++i)
    {
        size_t off = 12 + i * 8;
        write_u16_be(payload, off, entries[i].beat_number);
        write_u16_be(payload, off + 2, entries[i].tempo_x100);
        write_u32_be(payload, off + 4, entries[i].time_ms);
    }

    return payload;
}

// =========================================================================
// PVBR — VBR seek table (zeros for lossless)
//   u1(4) + 400 × idx(4) + u2(4) = 1608 bytes
// =========================================================================

std::vector<uint8_t> build_pvbr_payload()
{
    // 4 + 1600 + 4 = 1608
    return std::vector<uint8_t>(1608, 0);
}

// =========================================================================
// PWAV — mono waveform overview
//   len_data(4) + 0x00010000(4) + data(len_data)
//   Each byte: [whiteness:3][height:5]
// =========================================================================

std::vector<uint8_t> build_pwav_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = waveform.empty() ? 0
        : static_cast<uint32_t>(std::min(waveform.size(), size_t{1200}));

    std::vector<uint8_t> payload(8 + count, 0);
    write_u32_be(payload, 0, count);
    write_u32_be(payload, 4, 0x00010000);

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t src = waveform.size() * i / count;
        payload[8 + i] = convert::to_pwav_byte(waveform[src]);
    }
    return payload;
}

// =========================================================================
// PWV2 — small mono waveform
//   len_data(4) + 0x00010000(4) + data(len_data)
//   Height in low 4 bits (0-15).
// =========================================================================

std::vector<uint8_t> build_pwv2_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = waveform.empty() ? 0
        : static_cast<uint32_t>(std::min(waveform.size(), size_t{100}));

    std::vector<uint8_t> payload(8 + count, 0);
    write_u32_be(payload, 0, count);
    write_u32_be(payload, 4, 0x00010000);

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t src = waveform.size() * i / count;
        payload[8 + i] = convert::to_pwv2_byte(waveform[src]);
    }
    return payload;
}

// =========================================================================
// PWV3 — colour waveform scroll (1 byte/entry)
//   len_entry_bytes=1(4) + len_entries(4) + 0x00960000(4) + data(len_entries)
//   Each byte: [colour:3][height:5]
// =========================================================================

std::vector<uint8_t> build_pwv3_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = static_cast<uint32_t>(waveform.size());

    std::vector<uint8_t> payload(12 + count, 0);
    write_u32_be(payload, 0, 1);
    write_u32_be(payload, 4, count);
    write_u32_be(payload, 8, 0x00960000);

    for (uint32_t i = 0; i < count; ++i)
        payload[12 + i] = convert::to_pwv3_byte(waveform[i]);

    return payload;
}

// =========================================================================
// PWV4 — colour waveform preview (6 bytes/entry)
//   len_entry_bytes=6(4) + len_entries(4) + unknown(4) + data(6×len_entries)
//   Entry: [colour:3 height:5](u8) + luminance(u8) + blue_inv(u8) +
//          red(u8) + green(u8) + blue_height(u8)
// =========================================================================

std::vector<uint8_t> build_pwv4_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = waveform.empty() ? 0
        : static_cast<uint32_t>(std::min(waveform.size(), size_t{1200}));
    size_t data_size = count * 6;

    std::vector<uint8_t> payload(12 + data_size, 0);
    write_u32_be(payload, 0, 6);
    write_u32_be(payload, 4, count);

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t src = waveform.size() * i / count;
        auto e = convert::to_pwv4_entry(waveform[src]);
        size_t off = 12 + i * 6;
        payload[off] = e.ch;
        payload[off + 1] = e.luminance;
        payload[off + 2] = e.blue_inv;
        payload[off + 3] = e.red;
        payload[off + 4] = e.green;
        payload[off + 5] = e.blue_height;
    }
    return payload;
}

// =========================================================================
// PWV5 — colour waveform detail (2 bytes/entry)
//   len_entry_bytes=2(4) + len_entries(4) + unknown(4) + data(2×len_entries)
//   Entry (16-bit BE): [R:3][G:3][B:3][height:5][00:2]
// =========================================================================

std::vector<uint8_t> build_pwv5_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = static_cast<uint32_t>(waveform.size());
    size_t data_size = count * 2;

    std::vector<uint8_t> payload(12 + data_size, 0);
    write_u32_be(payload, 0, 2);
    write_u32_be(payload, 4, count);

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 2;
        write_u16_be(payload, off, convert::to_pwv5_entry(waveform[i]));
    }
    return payload;
}

// =========================================================================
// PWV6 — 3-band waveform preview (3 bytes/entry)
//   len_entry_bytes=3(4) + len_entries(4) + data(3×len_entries)
//   Entry: mid(1) + high(1) + low(1)
// =========================================================================

std::vector<uint8_t> build_pwv6_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = waveform.empty() ? 0
        : static_cast<uint32_t>(std::min(waveform.size(), size_t{1200}));
    size_t data_size = count * 3;

    std::vector<uint8_t> payload(8 + data_size, 0);
    write_u32_be(payload, 0, 3);
    write_u32_be(payload, 4, count);

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t src = waveform.size() * i / count;
        auto e = convert::to_pwv67_entry(waveform[src]);
        size_t off = 8 + i * 3;
        payload[off] = e.mid;
        payload[off + 1] = e.high;
        payload[off + 2] = e.low;
    }
    return payload;
}

// =========================================================================
// PWV7 — 3-band waveform detail (3 bytes/entry)
//   len_entry_bytes=3(4) + len_entries(4) + 0x00960000(4) + data(3×len_entries)
// =========================================================================

std::vector<uint8_t> build_pwv7_payload(
    const std::vector<waveform_entry>& waveform)
{
    uint32_t count = static_cast<uint32_t>(waveform.size());
    size_t data_size = count * 3;

    std::vector<uint8_t> payload(12 + data_size, 0);
    write_u32_be(payload, 0, 3);
    write_u32_be(payload, 4, count);
    write_u32_be(payload, 8, 0x00960000);

    for (uint32_t i = 0; i < count; ++i)
    {
        auto e = convert::to_pwv67_entry(waveform[i]);
        size_t off = 12 + i * 3;
        payload[off] = e.mid;
        payload[off + 1] = e.high;
        payload[off + 2] = e.low;
    }
    return payload;
}

// =========================================================================
// PQT2 — extended beat grid (empty)
//   pad(4) + u1(4) + pad(4) + 2×AnlzQuantizeTick(2×8=16) + count(4) +
//   u3(4) + u4(4) + u5(4) = 56 bytes, entry_count = 0
// =========================================================================

std::vector<uint8_t> build_empty_pqt2_payload()
{
    return std::vector<uint8_t>(56, 0);
}

}  // namespace djinterop::onelibrary::anlz