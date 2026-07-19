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

/// Data needed to write ANLZ files for a single track.
struct anlz_track_data
{
    /// Volume-relative audio path, e.g. `/Contents/Track.flac`.
    std::string relative_path;

    /// Track duration in seconds.
    double duration_secs = 0;

    /// Sample rate in Hz.
    double sample_rate = 44100;

    /// Average loudness (LUFS or similar). 0 means unknown.
    std::optional<double> average_loudness;

    /// Beat grid markers (index + sample_offset).
    /// When empty, an empty beat grid is written.
    std::vector<beatgrid_marker> beatgrid;

    /// Hot cues (label, sample_offset, colour).
    /// Indices correspond to hot cue slots 0-7.
    std::vector<std::optional<hot_cue>> hot_cues;

    /// Loops (label, start/end sample_offset, colour).
    /// Indices correspond to loop slots 0-7.
    std::vector<std::optional<loop>> loops;

    /// Waveform data (low/mid/high bands per entry).
    std::vector<waveform_entry> waveform;

    /// Main cue sample offset.
    std::optional<double> main_cue;
};

/// Write the complete ANLZ file set for a track.
///
/// Writes `ANLZ0000.DAT`, `ANLZ0000.EXT`, and `ANLZ0000.2EX` at the
/// hash-derived path under `.PIONEER/USBANLZ/` within the given volume root.
/// The minimal djay Pro tag set is used — empty cue containers, zero
/// waveforms, and beat grid derived from BPM.
///
/// \param volume_root Volume root directory (e.g. `/mnt/usb`).
/// \param track Track data.
void write_anlz_files(
    const std::string& volume_root, const anlz_track_data& track);

}  // namespace djinterop::onelibrary::anlz