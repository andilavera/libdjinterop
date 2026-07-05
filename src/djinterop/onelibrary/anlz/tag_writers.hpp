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

/// Build the PPTH payload: volume-relative audio path as UTF-16BE
/// with a mandatory trailing U+0000 (2 bytes).
std::vector<uint8_t> build_ppth_payload(const std::string& utf8_path);

/// Build an empty PCOB container (memory or hot).
/// container_type: 0 = memory, 1 = hot.
std::vector<uint8_t> build_empty_pcob_payload(uint32_t container_type);

/// Build a PCOB container with cue entries.
/// container_type: 0 = memory, 1 = hot.
/// cues: vector of (time_ms, loop_time_ms_or_0xFFFFFFFF).
///   hot_cue index is auto-assigned 1..N (or 0 for memory).
std::vector<uint8_t> build_pcob_payload(
    uint32_t container_type,
    const std::vector<std::pair<uint32_t, uint32_t>>& cues);

/// Build a PQTZ beat grid payload from BPM and track duration.
/// If bpm <= 0, produces a single-beat placeholder grid.
/// Internal structure: pad(4) + 0x00080000(4) + count(4) + count×8-byte entries.
std::vector<uint8_t> build_pqtz_payload(double bpm, double duration_secs);

/// Build a PVBR (VBR seek table) payload — all zeros for lossless.
/// Internal structure: u1(4) + 400×idx(4) + u2(4) = 1608 bytes.
std::vector<uint8_t> build_pvbr_payload();

/// Build a PWAV (mono waveform overview) payload.
/// count: number of entries (typically 400). Filled with a sawtooth pattern.
std::vector<uint8_t> build_pwav_payload(uint32_t count = 400);

/// Build a PWV2 (small mono waveform) payload.
/// count: number of entries (typically 100).
std::vector<uint8_t> build_pwv2_payload(uint32_t count = 100);

/// Build a PWV3 (colour waveform scroll) payload.
/// entries determined by `150 * duration_secs`.
std::vector<uint8_t> build_pwv3_payload(double duration_secs);

/// Build a PWV4 (colour waveform preview) payload, ~1200 entries.
std::vector<uint8_t> build_pwv4_payload(uint32_t count = 1200);

/// Build a PWV5 (colour waveform detail) payload.
/// entries determined by `150 * duration_secs`.
std::vector<uint8_t> build_pwv5_payload(double duration_secs);

/// Build a PWV6 (3-band waveform preview) payload, ~1200 entries.
std::vector<uint8_t> build_pwv6_payload(uint32_t count = 1200);

/// Build a PWV7 (3-band waveform detail) payload.
/// entries determined by `150 * duration_secs`.
std::vector<uint8_t> build_pwv7_payload(double duration_secs);

/// Build an empty PQT2 (extended beat grid) payload.
/// Internal structure: pad(4) + u1(4) + pad(4) + 2×quantize_tick(16) + count(4) + u3(4) + u4(4) + u5(4).
/// 56 bytes total, entry_count=0.
std::vector<uint8_t> build_empty_pqt2_payload();

}  // namespace djinterop::onelibrary::anlz