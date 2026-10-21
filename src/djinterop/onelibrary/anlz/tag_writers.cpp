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
#include "format.hpp"
#include "utf.hpp"

namespace djinterop::onelibrary::anlz
{

// =========================================================================
// PPTH
// =========================================================================

std::vector<uint8_t> build_ppth_payload(const std::string& utf8_path)
{
    const auto units = utf::to_utf16(utf8_path);

    // Trailing NUL terminator is mandatory.
    std::vector<uint8_t> payload((units.size() + 1) * 2, 0);
    for (size_t i = 0; i < units.size(); ++i)
        write_u16_be(payload, i * 2, units[i]);

    return payload;
}

// =========================================================================
// PCOB
// =========================================================================

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
//   u1(4) + 400 × idx(4) + u2(4)
// =========================================================================

namespace
{
constexpr size_t pvbr_index_count = 400;
}

std::vector<uint8_t> build_pvbr_payload()
{
    return std::vector<uint8_t>(4 + pvbr_index_count * 4 + 4, 0);
}

// =========================================================================
// PWAV — mono waveform overview
//   len_data(4) + 0x00010000(4) + data(len_data)
//   Each byte: [whiteness:3][height:5]
// =========================================================================

namespace
{
constexpr size_t pwav_columns = 1200;
constexpr size_t pwv2_columns = 100;
constexpr size_t pwv4_columns = 1200;
constexpr size_t pwv6_columns = 1200;

size_t column_count(
    const std::vector<waveform_entry>& waveform, size_t max_columns)
{
    return waveform.empty() ? 0 : std::min(waveform.size(), max_columns);
}

size_t source_index(size_t source_size, size_t columns, size_t i)
{
    return source_size * i / columns;
}

template <typename ToByte>
std::vector<uint8_t> build_mono_preview(
    const std::vector<waveform_entry>& waveform, size_t max_columns,
    ToByte to_byte)
{
    const size_t columns = column_count(waveform, max_columns);
    std::vector<uint8_t> payload(8 + columns, 0);
    write_u32_be(payload, 0, static_cast<uint32_t>(columns));
    write_u32_be(payload, 4, 0x00010000);

    for (size_t i = 0; i < columns; ++i)
        payload[8 + i] =
            to_byte(waveform[source_index(waveform.size(), columns, i)]);

    return payload;
}
}  // anonymous namespace

std::vector<uint8_t> build_pwav_payload(
    const std::vector<waveform_entry>& waveform)
{
    return build_mono_preview(waveform, pwav_columns, convert::to_pwav_byte);
}

// =========================================================================
// PWV2 — small mono waveform
//   len_data(4) + 0x00010000(4) + data(len_data)
//   Height in low 4 bits (0-15).
// =========================================================================

std::vector<uint8_t> build_pwv2_payload(
    const std::vector<waveform_entry>& waveform)
{
    return build_mono_preview(waveform, pwv2_columns, convert::to_pwv2_byte);
}

// =========================================================================
// PWV3 — mono waveform detail (1 byte/entry)
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
    const size_t columns = column_count(waveform, pwv4_columns);

    std::vector<uint8_t> payload(12 + columns * 6, 0);
    write_u32_be(payload, 0, 6);
    write_u32_be(payload, 4, static_cast<uint32_t>(columns));

    for (size_t i = 0; i < columns; ++i)
    {
        auto e = convert::to_pwv4_entry(
            waveform[source_index(waveform.size(), columns, i)]);
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
    const size_t columns = column_count(waveform, pwv6_columns);

    std::vector<uint8_t> payload(8 + columns * 3, 0);
    write_u32_be(payload, 0, 3);
    write_u32_be(payload, 4, static_cast<uint32_t>(columns));

    for (size_t i = 0; i < columns; ++i)
    {
        auto e = convert::to_pwv67_entry(
            waveform[source_index(waveform.size(), columns, i)]);
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