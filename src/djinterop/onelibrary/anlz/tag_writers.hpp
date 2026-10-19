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

namespace djinterop::onelibrary::anlz
{

/// Build the PPTH payload: volume-relative audio path as UTF-16BE
/// with a mandatory trailing U+0000 (2 bytes).
std::vector<uint8_t> build_ppth_payload(const std::string& utf8_path);

/// Build an empty PCOB container (memory or hot).
/// container_type: 0 = memory, 1 = hot.
std::vector<uint8_t> build_empty_pcob_payload(uint32_t container_type);

/// Build a PCOB container with hot cue entries.
/// container_type: 1 = hot cues. Indices map to hot cue slots 0-7.
std::vector<uint8_t> build_pcob_payload(
    const std::vector<std::optional<hot_cue>>& cues, double sample_rate);

/// Build a PCOB container with loop entries.
/// container_type: 0 = memory cues. Indices map to loop slots 0-7.
std::vector<uint8_t> build_pcob_payload(
    const std::vector<std::optional<loop>>& loops, double sample_rate);

/// Build a PQTZ beat grid payload from beatgrid markers.
/// sample_rate is used to convert sample offsets to milliseconds.
/// If beatgrid is empty, produces a minimal single-beat placeholder.
std::vector<uint8_t> build_pqtz_payload(
    const std::vector<beatgrid_marker>& beatgrid, double sample_rate);

/// Build a PVBR (VBR seek table) payload — all zeros for lossless.
/// Internal structure: u1(4) + 400×idx(4) + u2(4) = 1608 bytes.
std::vector<uint8_t> build_pvbr_payload();

/// Build a PWAV (mono waveform overview) payload.
std::vector<uint8_t> build_pwav_payload(
    const std::vector<waveform_entry>& waveform);

/// Build a PWV2 (small mono waveform) payload.
std::vector<uint8_t> build_pwv2_payload(
    const std::vector<waveform_entry>& waveform);

/// Build a PWV3 (colour waveform scroll) payload.
std::vector<uint8_t> build_pwv3_payload(
    const std::vector<waveform_entry>& waveform);

/// Build a PWV4 (colour waveform preview) payload.
std::vector<uint8_t> build_pwv4_payload(
    const std::vector<waveform_entry>& waveform);

/// Build a PWV5 (colour waveform detail) payload.
std::vector<uint8_t> build_pwv5_payload(
    const std::vector<waveform_entry>& waveform);

/// Build a PWV6 (3-band waveform preview) payload.
std::vector<uint8_t> build_pwv6_payload(
    const std::vector<waveform_entry>& waveform);

/// Build a PWV7 (3-band waveform detail) payload.
std::vector<uint8_t> build_pwv7_payload(
    const std::vector<waveform_entry>& waveform);

/// Build an empty PQT2 (extended beat grid) payload.
std::vector<uint8_t> build_empty_pqt2_payload();

}  // namespace djinterop::onelibrary::anlz