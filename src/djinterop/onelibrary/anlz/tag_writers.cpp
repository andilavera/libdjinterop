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

#include <cmath>

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

// Generate a sawtooth waveform value (0–31, 5 bits) cycling over `cycle_len`
// entries.  Produces a clearly visible pattern.
uint8_t sawtooth_height(size_t i, size_t cycle_len)
{
    return static_cast<uint8_t>((i % cycle_len) * 31 / cycle_len);
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

std::vector<uint8_t> build_pcob_payload(
    uint32_t container_type,
    const std::vector<std::pair<uint32_t, uint32_t>>& cues)
{
    // Container header: type(4) + padding(2) + count(2) + memory_count(4).
    // Each entry is a PCPT sub-tag: 12-byte sub-header + 44-byte body = 56.
    constexpr size_t entry_size = 56;
    uint16_t count = static_cast<uint16_t>(cues.size());
    size_t payload_size = 12 + count * entry_size;
    std::vector<uint8_t> payload(payload_size, 0);

    write_u32_be(payload, 0, container_type);  // type
    // bytes 4-5: padding (0)
    write_u16_be(payload, 6, count);            // num_cues at correct offset
    // bytes 8-11: memory_count (0)

    for (size_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * entry_size;
        auto [time_ms, loop_time_ms] = cues[i];
        bool is_loop = loop_time_ms != 0xFFFFFFFF;
        uint32_t hot_cue_idx = container_type == 1
                                   ? static_cast<uint32_t>(i + 1)
                                   : 0;

        // PCPT sub-tag header.
        payload[off] = 'P';
        payload[off + 1] = 'C';
        payload[off + 2] = 'P';
        payload[off + 3] = 'T';
        write_u32_be(payload, off + 4, 28);  // len_header
        write_u32_be(payload, off + 8, 56);  // len_entry

        // Body starts at off + 12.
        size_t b = off + 12;
        write_u32_be(payload, b, hot_cue_idx);
        write_u32_be(payload, b + 4, 4);  // status = enabled
        write_u32_be(payload, b + 8, 0x00010000);
        // order_first: 0xFFFF for first, 0 for second, then 2,3,...
        uint16_t of = (i == 0) ? 0xFFFF
                     : (i == 1) ? 0
                                : static_cast<uint16_t>(i);
        write_u16_be(payload, b + 12, of);
        // order_last: 1,2,3,..., 0xFFFF for last
        uint16_t ol = (i == static_cast<size_t>(count) - 1)
                          ? 0xFFFF
                          : static_cast<uint16_t>(i + 1);
        write_u16_be(payload, b + 14, ol);
        payload[b + 16] = is_loop ? 2 : 1;  // type: 1=point, 2=loop
        // b+17: padding
        write_u16_be(payload, b + 18, 1000);
        write_u32_be(payload, b + 20, time_ms);
        write_u32_be(payload, b + 24, loop_time_ms);
        // b+28..b+55: 28 bytes padding (already 0)
    }

    // memory_count: signed int32, equals count for memory, 0 for hot.
    if (container_type == 0)
        write_u32_be(payload, 8, count);

    return payload;
}

// =========================================================================
// PQTZ — beat grid
//   pad(4) + 0x00080000(4) + count(4) + count × 8-byte entries
//   entry: bar_pos(u16) + tempo_bpmx100(u16) + time_ms(u32)
// =========================================================================

std::vector<uint8_t> build_pqtz_payload(double bpm, double duration_secs)
{
    uint16_t tempo = 0;
    uint32_t entry_count = 0;
    double beat_interval_ms = 0;

    if (bpm > 0 && duration_secs > 0)
    {
        tempo = static_cast<uint16_t>(bpm * 100.0);
        double beats = bpm * duration_secs / 60.0;
        entry_count = static_cast<uint32_t>(beats) + 1;
        if (entry_count > 5000) entry_count = 5000;
        beat_interval_ms = 60000.0 / bpm;
    }

    size_t total = 12 + entry_count * 8;
    std::vector<uint8_t> payload(total, 0);

    write_u32_be(payload, 4, 0x00080000);
    write_u32_be(payload, 8, entry_count);

    for (uint32_t i = 0; i < entry_count; ++i)
    {
        size_t off = 12 + i * 8;
        write_u16_be(payload, off, static_cast<uint16_t>(i % 4));
        write_u16_be(payload, off + 2, tempo);
        write_u32_be(payload, off + 4,
                     static_cast<uint32_t>(i * beat_interval_ms));
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
// =========================================================================

std::vector<uint8_t> build_pwav_payload(uint32_t count)
{
    // 4 + 4 + count
    std::vector<uint8_t> payload(8 + count, 0);
    write_u32_be(payload, 0, count);
    write_u32_be(payload, 4, 0x00010000);
    for (uint32_t i = 0; i < count; ++i)
        payload[8 + i] = sawtooth_height(i, count / 4);
    return payload;
}

// =========================================================================
// PWV2 — small mono waveform
//   len_data(4) + 0x00010000(4) + data(len_data)
// =========================================================================

std::vector<uint8_t> build_pwv2_payload(uint32_t count)
{
    // Same structure as PWAV.
    std::vector<uint8_t> payload(8 + count, 0);
    write_u32_be(payload, 0, count);
    write_u32_be(payload, 4, 0x00010000);
    for (uint32_t i = 0; i < count; ++i)
        payload[8 + i] = sawtooth_height(i, count / 2);
    return payload;
}

// =========================================================================
// PWV3 — colour waveform scroll (1 byte/entry)
//   len_entry_bytes=1(4) + len_entries(4) + 0x00960000(4) + data(len_entries)
//   Each byte: [colour:3][height:5]
// =========================================================================

std::vector<uint8_t> build_pwv3_payload(double duration_secs)
{
    uint32_t count = static_cast<uint32_t>(duration_secs * 150.0);
    if (count < 1500) count = 1500;

    std::vector<uint8_t> payload(12 + count, 0);
    write_u32_be(payload, 0, 1);        // len_entry_bytes
    write_u32_be(payload, 4, count);    // len_entries
    write_u32_be(payload, 8, 0x00960000);

    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t height = sawtooth_height(i, 64);
        uint8_t colour = 7;  // white
        payload[12 + i] = static_cast<uint8_t>((colour << 5) | height);
    }
    return payload;
}

// =========================================================================
// PWV4 — colour waveform preview (6 bytes/entry), ~1200 entries
//   len_entry_bytes=6(4) + len_entries(4) + unknown(4) + data(6×len_entries)
//   Entry: [colour:3 height:5](u8) + luminance(u8) + blue_inv(u8) +
//          red(u8) + green(u8) + blue_height(u8)
// =========================================================================

std::vector<uint8_t> build_pwv4_payload(uint32_t count)
{
    size_t data_size = count * 6;
    std::vector<uint8_t> payload(12 + data_size, 0);
    write_u32_be(payload, 0, 6);        // len_entry_bytes
    write_u32_be(payload, 4, count);    // len_entries
    write_u32_be(payload, 8, 0);        // unknown

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 6;
        uint8_t height = sawtooth_height(i, 64);
        uint8_t colour = 7;
        payload[off] = static_cast<uint8_t>((colour << 5) | height);
        payload[off + 1] = 255;   // luminance
        payload[off + 2] = 0;     // blue_inv
        payload[off + 3] = 255;   // red
        payload[off + 4] = 255;   // green
        payload[off + 5] = height; // blue_height
    }
    return payload;
}

// =========================================================================
// PWV5 — colour waveform detail (2 bytes/entry)
//   len_entry_bytes=2(4) + len_entries(4) + unknown(4) + data(2×len_entries)
//   Entry (16-bit BE): [R:3][G:3][B:3][height:5][00:2]
// =========================================================================

std::vector<uint8_t> build_pwv5_payload(double duration_secs)
{
    uint32_t count = static_cast<uint32_t>(duration_secs * 150.0);
    if (count < 1500) count = 1500;

    size_t data_size = count * 2;
    std::vector<uint8_t> payload(12 + data_size, 0);
    write_u32_be(payload, 0, 2);        // len_entry_bytes
    write_u32_be(payload, 4, count);    // len_entries
    write_u32_be(payload, 8, 0);        // unknown

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 2;
        uint8_t height = sawtooth_height(i, 64);
        // RGB = 7,7,7 (white), height in bits [4:0], 00 in bits [1:0]
        uint16_t entry = static_cast<uint16_t>(
            (7u << 13) | (7u << 10) | (7u << 7) | (height << 2));
        write_u16_be(payload, off, entry);
    }
    return payload;
}

// =========================================================================
// PWV6 — 3-band waveform preview (3 bytes/entry), ~1200 entries
//   len_entry_bytes=3(4) + len_entries(4) + data(3×len_entries)
//   Entry: low(1) + mid(1) + high(1)
// =========================================================================

std::vector<uint8_t> build_pwv6_payload(uint32_t count)
{
    size_t data_size = count * 3;
    std::vector<uint8_t> payload(8 + data_size, 0);
    write_u32_be(payload, 0, 3);        // len_entry_bytes
    write_u32_be(payload, 4, count);    // len_entries

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 8 + i * 3;
        payload[off] = sawtooth_height(i, 64);       // low
        payload[off + 1] = sawtooth_height(i + 21, 64);  // mid (phase-shifted)
        payload[off + 2] = sawtooth_height(i + 42, 64);  // high (phase-shifted)
    }
    return payload;
}

// =========================================================================
// PWV7 — 3-band waveform detail (3 bytes/entry)
//   len_entry_bytes=3(4) + len_entries(4) + 0x00960000(4) + data(3×len_entries)
// =========================================================================

std::vector<uint8_t> build_pwv7_payload(double duration_secs)
{
    uint32_t count = static_cast<uint32_t>(duration_secs * 150.0);
    if (count < 1500) count = 1500;

    size_t data_size = count * 3;
    std::vector<uint8_t> payload(12 + data_size, 0);
    write_u32_be(payload, 0, 3);         // len_entry_bytes
    write_u32_be(payload, 4, count);     // len_entries
    write_u32_be(payload, 8, 0x00960000);

    for (uint32_t i = 0; i < count; ++i)
    {
        size_t off = 12 + i * 3;
        payload[off] = sawtooth_height(i, 64);
        payload[off + 1] = sawtooth_height(i + 21, 64);
        payload[off + 2] = sawtooth_height(i + 42, 64);
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