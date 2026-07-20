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
#include <optional>
#include <string>
#include <vector>

#include <djinterop/performance_data.hpp>

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

/// Read a PQTZ beat grid tag and return beatgrid markers.
///
/// Parses the payload: pad(4) + 0x80000(u4) + count(u4) + entries.
/// Each 8-byte entry: beat_number(u2), tempo_x100(u2), time_ms(u4).
/// The beat index is the entry position (0-based).  Sample offsets
/// are derived from time_ms using the given sample rate.
///
/// \param payload Raw PQTZ payload bytes.
/// \param sample_rate Sample rate in Hz for time_ms → sample conversion.
/// \return Beat grid markers.
std::vector<beatgrid_marker> read_pqtz(
    const std::vector<uint8_t>& payload, double sample_rate);

/// Parsed hot cues and loops from PCOB tag(s).
struct pcob_cues
{
    std::vector<std::optional<hot_cue>> hot_cues;
    std::vector<std::optional<loop>> loops;
};

/// Read a PCOB cue tag and return hot cues and/or loops.
///
/// Parses the payload: type(u4) + unk(u2) + count(u2) + memory_count(u4)
/// + entries.  Each entry is a PCPT sub-tag whose `len_entry` field
/// gives its size.
///
/// For type=1 (hot cues), entries map to `hot_cues` by `hot_cue` index
/// (1-based → 0-based slot).  For type=0 (memory), entries with
/// type=2 (loop) map to `loops`; point cues are ignored (no djinterop
/// representation).
///
/// \param payload Raw PCOB payload bytes.
/// \param sample_rate Sample rate in Hz for time_ms → sample conversion.
/// \return Parsed hot cues and loops.
pcob_cues read_pcob(
    const std::vector<uint8_t>& payload, double sample_rate);

// =========================================================================
// Waveform tag readers
// =========================================================================

/// Read a PWAV mono waveform tag (1 byte/entry: [whiteness:3][height:5]).
/// Header: count(u4) + 0x00010000(u4) + data.
std::vector<waveform_entry> read_pwav(const std::vector<uint8_t>& payload);

/// Read a PWV2 tiny mono waveform tag (1 byte/entry: height in low 4 bits).
/// Header: count(u4) + 0x00010000(u4) + data.
std::vector<waveform_entry> read_pwv2(const std::vector<uint8_t>& payload);

/// Read a PWV3 colour waveform tag (1 byte/entry: [colour:3][height:5]).
/// Header: 1(u4) + count(u4) + 0x00960000(u4) + data.
std::vector<waveform_entry> read_pwv3(const std::vector<uint8_t>& payload);

/// Read a PWV4 colour waveform preview (6 bytes/entry).
/// Header: 6(u4) + count(u4) + unknown(u4) + data.
std::vector<waveform_entry> read_pwv4(const std::vector<uint8_t>& payload);

/// Read a PWV5 colour waveform detail (2 bytes/entry, 16-bit BE).
/// Header: 2(u4) + count(u4) + unknown(u4) + data.
std::vector<waveform_entry> read_pwv5(const std::vector<uint8_t>& payload);

/// Read a PWV6 3-band waveform preview (3 bytes/entry: mid, high, low).
/// Header: 3(u4) + count(u4) + data.
std::vector<waveform_entry> read_pwv6(const std::vector<uint8_t>& payload);

/// Read a PWV7 3-band waveform detail (3 bytes/entry: mid, high, low).
/// Header: 3(u4) + count(u4) + 0x00960000(u4) + data.
std::vector<waveform_entry> read_pwv7(const std::vector<uint8_t>& payload);

}  // namespace djinterop::onelibrary::anlz
