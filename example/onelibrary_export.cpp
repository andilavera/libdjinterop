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

#include <djinterop/djinterop.hpp>

#include "djinterop/onelibrary/anlz/anlz_writer.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    using namespace std::string_literals;
    namespace fs = std::filesystem;
    namespace onelib = djinterop::onelibrary;
    namespace anlz = djinterop::onelibrary::anlz;

    std::string dir = argc >= 2 ? argv[1] : "tmp/ex_usb"s;

    // Clean up any previous run.
    std::error_code ec;
    if (fs::exists(dir))
    {
        fs::remove_all(dir, ec);
    }

    // Create the Contents directory and copy the audio file into it.
    auto contents_dir = fs::path{dir} / "Contents";
    fs::create_directories(contents_dir, ec);
    if (ec)
    {
        std::cerr << "Error: cannot create " << contents_dir << ": "
                  << ec.message() << "\n";
        return 1;
    }

    // Locate the example audio file next to this executable, or in the
    // source tree.
    const char* audio_name = "01 Lights Burn Dimmer.flac";
    auto audio_src = fs::path{"/workspace/libdjinterop/example"} / audio_name;
    auto audio_dst = contents_dir / audio_name;

    if (!fs::exists(audio_src))
    {
        std::cerr << "Error: cannot find " << audio_src << "\n";
        return 1;
    }

    fs::copy_file(audio_src, audio_dst, ec);
    if (ec)
    {
        std::cerr << "Error: cannot copy audio file: " << ec.message()
                  << "\n";
        return 1;
    }

    // Copy artwork.
    auto art_src = fs::path{"/workspace/libdjinterop/PIONEER_DJAY_ONE/Artwork/00001/b1.jpg"};
    auto art_dir = fs::path{dir} / ".PIONEER/Artwork/00001";
    fs::create_directories(art_dir, ec);
    auto art_dst = art_dir / "b1.jpg";
    fs::copy_file(art_src, art_dst, ec);
    if (ec)
    {
        std::cerr << "Error: cannot copy artwork: " << ec.message() << "\n";
        return 1;
    }

    try
    {
        // Create the library.
        auto lib = onelib::onelibrary::create(dir);

        onelib::track_info track;
        track.title = "Lights Burn Dimmer";
        track.artist = "Fred again..";
        track.album = "USB";
        track.album_artist = "Fred again..";
        track.genre = "Electronic";
        track.composer = "Olly Burden";
        track.lyricist = "Olly Burden";
        track.year = 2022;
        track.duration_secs = 261;
        track.track_number = 1;
        track.bpm = 106;
        track.bitrate = 1024;
        track.bit_depth = 16;
        track.sample_rate = 44100;
        track.file_type = 5;  // FLAC
        track.isrc = "GBAHS2501741";
        track.relative_path = "/Contents/01 Lights Burn Dimmer.flac";
        track.file_size_bytes =
            static_cast<int64_t>(fs::file_size(audio_dst));

        // Register artwork.
        auto art_id = lib.add_artwork(
            "/.PIONEER/Artwork/00001/b1.jpg");
        track.image_id = art_id;

        int64_t id = lib.add_track(track);
        lib.verify();

        // Write ANLZ files for the track.
        anlz::anlz_track_data anlz_data;
        anlz_data.relative_path = track.relative_path;
        anlz_data.duration_secs = track.duration_secs;
        anlz_data.sample_rate = static_cast<double>(track.sample_rate);

        // Add a beat grid starting at 0 with the track BPM.
        {
            double samples_per_beat = 60.0 / track.bpm * track.sample_rate;
            anlz_data.beatgrid.push_back(
                djinterop::beatgrid_marker{0, 0.0});
            for (int beat = 1; beat <= 100; ++beat)
            {
                anlz_data.beatgrid.push_back(
                    djinterop::beatgrid_marker{
                        beat, beat * samples_per_beat});
            }
        }

        // Add cue points: 4 hot cues + 4 memory cues (loops).
        // Starting at 5 seconds, 1 second apart.
        // Last memory cue is a 2-second loop.
        for (int i = 0; i < 4; ++i)
        {
            double sample_offset = (5.0 + i) * track.sample_rate;
            djinterop::hot_cue hc;
            hc.label = "Cue " + std::to_string(i + 1);
            hc.sample_offset = sample_offset;
            hc.color = djinterop::pad_color{255, 0, 0, 255};
            anlz_data.hot_cues.push_back(hc);

            djinterop::loop lp;
            lp.label = "Loop " + std::to_string(i + 1);
            lp.start_sample_offset = sample_offset;
            if (i == 3)
            {
                // 2-second loop.
                lp.end_sample_offset = sample_offset +
                    2.0 * track.sample_rate;
            }
            else
            {
                lp.end_sample_offset = sample_offset;
            }
            lp.color = djinterop::pad_color{0, 0, 255, 255};
            anlz_data.loops.push_back(lp);
        }

        anlz::write_anlz_files(dir, anlz_data);

        std::cout << "Created OneLibrary export with 1 track\n"
                  << "  Directory: " << dir << "\n"
                  << "  Track count: " << lib.track_count() << "\n"
                  << "  content_id: " << id << "\n";

        // Create a playlist and add the track.
        auto pl_id = lib.create_playlist("My Playlist");
        lib.add_track_to_playlist(pl_id, id);
        std::cout << "  Playlists: " << lib.root_playlists().size()
                  << " root, '" << lib.playlist_children(0).size()
                  << "' children, track added to playlist " << pl_id
                  << "\n";

        // Read back the track to verify round-trip.
        auto read = lib.get_track(id);
        if (read)
        {
            std::cout << "\nRound-trip read:\n"
                      << "  Title:  " << read->title << "\n"
                      << "  Artist: " << read->artist << "\n"
                      << "  Album:  "
                      << read->album.value_or("(none)") << "\n"
                      << "  Genre:  "
                      << read->genre.value_or("(none)") << "\n"
                      << "  BPM:    " << read->bpm << "\n"
                      << "  ISRC:   "
                      << read->isrc.value_or("(none)") << "\n";
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nLayout:\n"
              << "  " << dir << "/\n"
              << "    .PIONEER/\n"
              << "      rekordbox/\n"
              << "        exportLibrary.db\n"
              << "    Contents/\n"
              << "      " << audio_name << "\n"
              << "\nCopy the entire " << dir
              << " contents to a USB drive root.\n";

    return 0;
}