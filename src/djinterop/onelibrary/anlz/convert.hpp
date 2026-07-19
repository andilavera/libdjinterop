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
#include <vector>

#include <djinterop/performance_data.hpp>

namespace djinterop::onelibrary::anlz::convert
{

// =========================================================================
// Sample offset ↔ millisecond conversion
// =========================================================================

/// Convert a sample offset to milliseconds at the given sample rate.
inline uint32_t sample_offset_to_ms(double sample_offset, double sample_rate)
{
    return static_cast<uint32_t>(sample_offset * 1000.0 / sample_rate);
}

// =========================================================================
// PQTZ — beat grid entries
// =========================================================================

/// Intermediate representation of a single PQTZ beat grid entry, ready to
/// be serialized into the 8-byte binary format.
struct pqtz_entry
{
    /// Beat number within the bar (1–4).
    uint16_t beat_number;

    /// Tempo in BPM × 100 for this beat.
    uint16_t tempo_x100;

    /// Time since track start in milliseconds.
    uint32_t time_ms;
};

/// Convert a beatgrid to PQTZ entries.
///
/// For each beatgrid marker, computes beat_number, tempo_x100 (from
/// adjacent markers), and time_ms.
///
/// \param beatgrid Beat grid markers (index + sample_offset).
/// \param sample_rate Sample rate in Hz.
/// \return PQTZ entries ready for serialization.
std::vector<pqtz_entry> to_pqtz_entries(
    const std::vector<beatgrid_marker>& beatgrid, double sample_rate);

// =========================================================================
// PCOB — cue / loop entry data
// =========================================================================

/// Intermediate data for one PCOB (PCPT sub-tag) entry.
struct pcob_entry_data
{
    /// Hot cue index (1-based, or 0 for memory/loop cues).
    uint32_t hot_cue_idx = 0;

    /// True if this entry represents a loop vs a point cue.
    bool is_loop = false;

    /// Time in milliseconds from track start.
    uint32_t time_ms = 0;

    /// Loop duration in milliseconds, or 0xFFFFFFFF if not a loop.
    uint32_t loop_time_ms = 0xFFFFFFFF;
};

/// Convert a hot cue to PCOB entry data.
///
/// \param cue The hot cue (may be std::nullopt for an empty slot).
/// \param slot The slot index (0-based, maps to hot cue 1–8).
/// \param sample_rate Sample rate in Hz.
/// \return Entry data, or std::nullopt if the cue is empty.
std::optional<pcob_entry_data> to_pcob_hot_cue_data(
    const std::optional<hot_cue>& cue, size_t slot, double sample_rate);

/// Convert a loop to PCOB entry data.
///
/// \param lp The loop (may be std::nullopt for an empty slot).
/// \param sample_rate Sample rate in Hz.
/// \return Entry data, or std::nullopt if the loop is empty.
std::optional<pcob_entry_data> to_pcob_loop_data(
    const std::optional<loop>& lp, double sample_rate);

// =========================================================================
// Waveform — per-entry byte conversion
// =========================================================================

/// Compute the maximum of the three band values.
inline uint8_t max_band_value(const waveform_entry& e)
{
    return std::max(e.low.value, std::max(e.mid.value, e.high.value));
}

/// Compute the maximum of the three band opacities.
inline uint8_t max_band_opacity(const waveform_entry& e)
{
    return std::max(e.low.opacity, std::max(e.mid.opacity, e.high.opacity));
}

/// Convert a waveform entry to a PWAV byte: [whiteness:3][height:5].
inline uint8_t to_pwav_byte(const waveform_entry& e)
{
    uint8_t height = max_band_value(e) >> 3;           // 0–31
    uint8_t whiteness = max_band_opacity(e) >> 5;      // 0–7
    return static_cast<uint8_t>((whiteness << 5) | height);
}

/// Convert a waveform entry to a PWV2 byte: height in low 4 bits (0–15).
inline uint8_t to_pwv2_byte(const waveform_entry& e)
{
    return max_band_value(e) >> 4;  // 0–15
}

/// Convert a waveform entry to a PWV3 byte: [colour:3][height:5].
inline uint8_t to_pwv3_byte(const waveform_entry& e)
{
    uint8_t height = max_band_value(e) >> 3;           // 0–31
    uint8_t colour = e.low.value > 127 ? 7 : 4;        // white or blue
    return static_cast<uint8_t>((colour << 5) | height);
}

/// Convert a waveform entry to a PWV4 6-byte entry.
struct pwv4_entry
{
    uint8_t ch;           // [colour:3][height:5]
    uint8_t luminance;    // mid opacity
    uint8_t blue_inv;     // 255 - high opacity
    uint8_t red;          // low value
    uint8_t green;        // mid value
    uint8_t blue_height;  // height (0–31)
};

inline pwv4_entry to_pwv4_entry(const waveform_entry& e)
{
    uint8_t height = max_band_value(e) >> 3;
    return {
        static_cast<uint8_t>((7u << 5) | height),
        e.mid.opacity,
        static_cast<uint8_t>(255 - e.high.opacity),
        e.low.value,
        e.mid.value,
        height,
    };
}

/// Convert a waveform entry to a PWV5 16-bit entry.
/// Format: [R:3][G:3][B:3][height:5][00:2]
inline uint16_t to_pwv5_entry(const waveform_entry& e)
{
    uint8_t height = max_band_value(e) >> 3;
    uint8_t r = e.low.value >> 5;    // 0–7
    uint8_t g = e.mid.value >> 5;
    uint8_t b = e.high.value >> 5;
    return static_cast<uint16_t>(
        (r << 13) | (g << 10) | (b << 7) | (height << 2));
}

/// Convert a waveform entry to PWV6/PWV7 3-byte data: mid, high, low.
struct pwv67_entry
{
    uint8_t mid;
    uint8_t high;
    uint8_t low;
};

inline pwv67_entry to_pwv67_entry(const waveform_entry& e)
{
    return {e.mid.value, e.high.value, e.low.value};
}

}  // namespace djinterop::onelibrary::anlz::convert