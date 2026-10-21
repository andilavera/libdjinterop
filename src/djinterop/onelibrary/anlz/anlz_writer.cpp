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

#include "anlz_writer.hpp"

#include <filesystem>

#include "hash.hpp"
#include "pmai_writer.hpp"
#include "tag_writers.hpp"

namespace djinterop::onelibrary::anlz
{

namespace
{
namespace fs = std::filesystem;

tag_section make_tag(
    const std::string& id, uint32_t len_header,
    std::vector<uint8_t> payload)
{
    return {id, len_header, std::move(payload)};
}

tag_section make_ppth(const std::string& path)
{
    return make_tag("PPTH", 16, build_ppth_payload(path));
}
}  // anonymous namespace

void write_anlz_files(
    const std::string& volume_root, const anlz_track_data& track)
{
    auto anlz_path = compute_anlz_path(track.relative_path);
    auto dir = fs::path{volume_root} / ".PIONEER/USBANLZ" /
               anlz_path.to_directory();
    fs::create_directories(dir);

    double sr = track.sample_rate > 0 ? track.sample_rate : 44100.0;

    // ---- ANLZ0000.DAT ----
    {
        std::vector<tag_section> tags;
        tags.push_back(make_ppth(track.relative_path));
        tags.push_back(make_tag("PVBR", 16, build_pvbr_payload()));
        tags.push_back(
            make_tag("PQTZ", 24, build_pqtz_payload(track.beatgrid, sr)));
        tags.push_back(
            make_tag("PWAV", 20, build_pwav_payload(track.waveform)));
        tags.push_back(
            make_tag("PWV2", 20, build_pwv2_payload(track.waveform)));

        // Empty containers are produced by the builders themselves.
        tags.push_back(
            make_tag("PCOB", 24, build_pcob_payload(track.hot_cues, sr)));
        tags.push_back(
            make_tag("PCOB", 24, build_pcob_payload(track.loops, sr)));

        write_pmai_file((dir / "ANLZ0000.DAT").string(), tags);
    }

    // ---- ANLZ0000.EXT ----
    {
        std::vector<tag_section> tags;
        tags.push_back(make_ppth(track.relative_path));
        tags.push_back(
            make_tag("PWV3", 24, build_pwv3_payload(track.waveform)));
        tags.push_back(
            make_tag("PWV4", 24, build_pwv4_payload(track.waveform)));
        tags.push_back(
            make_tag("PWV5", 24, build_pwv5_payload(track.waveform)));
        tags.push_back(
            make_tag("PQT2", 56, build_empty_pqt2_payload()));

        write_pmai_file((dir / "ANLZ0000.EXT").string(), tags);
    }

    // ---- ANLZ0000.2EX ----
    {
        std::vector<tag_section> tags;
        tags.push_back(make_ppth(track.relative_path));
        tags.push_back(
            make_tag("PWV6", 20, build_pwv6_payload(track.waveform)));
        tags.push_back(
            make_tag("PWV7", 24, build_pwv7_payload(track.waveform)));

        write_pmai_file((dir / "ANLZ0000.2EX").string(), tags);
    }
}

}  // namespace djinterop::onelibrary::anlz