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

#include "track_impl.hpp"

#include <utility>

#include <djinterop/database.hpp>
#include <djinterop/exceptions.hpp>
#include <djinterop/onelibrary/v1/content_table.hpp>
#include <djinterop/onelibrary/v1/playlist_table.hpp>

#include "../../util/filesystem.hpp"
#include "crate_impl.hpp"
#include "database_impl.hpp"
#include "track_conversion.hpp"

namespace djinterop::onelibrary::v1
{
// Loudness, beatgrids, waveforms, hot cues and loops are absent throughout:
// rekordbox leaves them in the ANLZ files that `analysisDataFilePath` points
// at, and exports an empty `cue` table.  Their accessors read as no value
// rather than raising.

namespace
{
/// The columns a track snapshot carries, as a content row.
///
/// `analysis_path` and `color_id` are left unset: neither is part of a
/// snapshot, and so neither is written from one.
content_row to_content_row(const track_snapshot& snapshot)
{
    if (!snapshot.relative_path)
    {
        throw invalid_track_snapshot{
            "Snapshot does not contain a populated `relative_path` field, "
            "which is required to write a track"};
    }

    content_row row;
    row.title = snapshot.title;
    row.artist = snapshot.artist;
    row.composer = snapshot.composer;
    row.album = snapshot.album;
    row.genre = snapshot.genre;
    row.label = snapshot.publisher;
    if (snapshot.key)
        row.key = to_key_name(*snapshot.key);
    row.comment = snapshot.comment;
    if (snapshot.bpm)
        row.bpm_x100 = static_cast<int64_t>(*snapshot.bpm * 100);
    if (snapshot.duration)
        row.length = std::chrono::duration_cast<std::chrono::seconds>(
            *snapshot.duration);
    if (snapshot.track_number)
        row.track_number = *snapshot.track_number;
    if (snapshot.year)
        row.release_year = *snapshot.year;
    if (snapshot.rating)
        row.rating_stars = *snapshot.rating / 20;
    row.path = to_device_path(*snapshot.relative_path);
    if (snapshot.file_bytes)
        row.file_size = static_cast<int64_t>(*snapshot.file_bytes);
    if (snapshot.bitrate)
        row.bitrate = *snapshot.bitrate;
    if (snapshot.sample_rate)
        row.sampling_rate = static_cast<int64_t>(*snapshot.sample_rate);

    return row;
}

/// The metadata fields of one row, over another.
void apply_metadata(content_row& row, const content_row& metadata)
{
    row.title = metadata.title;
    row.artist = metadata.artist;
    row.composer = metadata.composer;
    row.album = metadata.album;
    row.genre = metadata.genre;
    row.label = metadata.label;
    row.key = metadata.key;
    row.comment = metadata.comment;
    row.bpm_x100 = metadata.bpm_x100;
    row.length = metadata.length;
    row.track_number = metadata.track_number;
    row.release_year = metadata.release_year;
    row.rating_stars = metadata.rating_stars;
    row.path = metadata.path;
    row.file_size = metadata.file_size;
    row.bitrate = metadata.bitrate;
    row.sampling_rate = metadata.sampling_rate;
}

/// Read one row, apply a change to it and write it back.
///
/// Every setter goes through here, so each reads the row as it stands and
/// changes only its own field, leaving the rest -- including the fields the
/// track interface does not expose -- untouched.
template <typename Modify>
void modify_content(
    const std::shared_ptr<onelibrary_context>& context, int64_t id,
    Modify&& modify)
{
    auto row = content_table{context}.get(id);
    if (!row)
        throw track_deleted{id};

    modify(*row);
    content_table{context}.update(*row);
}

/// Refuse a field that the database holds no column for.
[[noreturn]] void no_column(const char* field)
{
    throw unsupported_operation{
        std::string{"OneLibrary has no column for a track's "} + field};
}
}  // anonymous namespace

track_impl::track_impl(
    std::shared_ptr<onelibrary_context> context, int64_t id) :
    djinterop::track_impl{id}, context_{std::move(context)}
{
}

track make_track(std::shared_ptr<onelibrary_context> context, int64_t id)
{
    return track{std::make_shared<track_impl>(std::move(context), id)};
}

track_snapshot track_impl::snapshot() const
{
    const auto row = content_table{context_}.get(id());
    if (!row)
        throw track_deleted{id()};

    return to_snapshot(*row);
}

bool track_impl::is_valid()
{
    return content_table{context_}.exists(id());
}

database track_impl::db()
{
    return database{std::make_shared<database_impl>(context_)};
}

std::vector<djinterop::crate> track_impl::containing_crates()
{
    // A crate and a playlist are the same row in this format, so the crates
    // containing a track are the playlists that hold it.
    std::vector<djinterop::crate> results;
    for (const auto& playlist_id :
         playlist_table{context_}.playlists_containing(id()))
        results.push_back(make_crate(context_, playlist_id));

    return results;
}

std::string track_impl::relative_path()
{
    const auto path = content_table{context_}.get_path(id());
    return path ? to_relative_path(*path) : std::string{};
}

std::string track_impl::filename()
{
    return util::get_filename(relative_path());
}

std::string track_impl::file_extension()
{
    return util::get_file_extension(relative_path()).value_or(std::string{});
}

std::optional<std::string> track_impl::album()
{
    return content_table{context_}.get_album(id());
}

std::optional<std::string> track_impl::artist()
{
    return content_table{context_}.get_artist(id());
}

std::optional<double> track_impl::average_loudness()
{
    return std::nullopt;
}

std::vector<beatgrid_marker> track_impl::beatgrid()
{
    return {};
}

std::optional<int> track_impl::bitrate()
{
    return to_positive_int(content_table{context_}.get_bitrate(id()));
}

std::optional<double> track_impl::bpm()
{
    return to_bpm(content_table{context_}.get_bpm_x100(id()));
}

std::optional<std::string> track_impl::comment()
{
    return content_table{context_}.get_comment(id());
}

std::optional<std::string> track_impl::composer()
{
    return content_table{context_}.get_composer(id());
}

std::optional<std::chrono::milliseconds> track_impl::duration()
{
    return to_duration(content_table{context_}.get_length(id()));
}

std::optional<std::string> track_impl::genre()
{
    return content_table{context_}.get_genre(id());
}

std::optional<hot_cue> track_impl::hot_cue_at(int)
{
    return std::nullopt;
}

std::vector<std::optional<hot_cue>> track_impl::hot_cues()
{
    return {};
}

std::optional<musical_key> track_impl::key()
{
    const auto name = content_table{context_}.get_key(id());
    if (!name)
        return std::nullopt;

    return parse_musical_key(*name);
}

std::optional<std::chrono::system_clock::time_point>
track_impl::last_played_at()
{
    // The database counts plays but does not record when the last one was.
    return std::nullopt;
}

std::optional<loop> track_impl::loop_at(int)
{
    return std::nullopt;
}

std::vector<std::optional<loop>> track_impl::loops()
{
    return {};
}

std::optional<double> track_impl::main_cue()
{
    return std::nullopt;
}

std::optional<std::string> track_impl::publisher()
{
    return content_table{context_}.get_label(id());
}

std::optional<int> track_impl::rating()
{
    return to_rating(content_table{context_}.get_rating_stars(id()));
}

std::optional<unsigned long long> track_impl::sample_count()
{
    const content_table content{context_};
    return to_sample_count(
        content.get_length(id()), content.get_sampling_rate(id()));
}

std::optional<double> track_impl::sample_rate()
{
    return to_sample_rate(content_table{context_}.get_sampling_rate(id()));
}

std::optional<std::string> track_impl::title()
{
    return content_table{context_}.get_title(id());
}

std::optional<int> track_impl::track_number()
{
    return to_positive_int(content_table{context_}.get_track_number(id()));
}

std::vector<waveform_entry> track_impl::waveform()
{
    return {};
}

std::optional<int> track_impl::year()
{
    return to_positive_int(content_table{context_}.get_release_year(id()));
}

void track_impl::update(const track_snapshot& snapshot)
{
    const auto metadata = to_content_row(snapshot);

    modify_content(
        context_, id(),
        [&](content_row& row) { apply_metadata(row, metadata); });
}

void track_impl::set_album(std::optional<std::string> album)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.album = std::move(album); });
}

void track_impl::set_artist(std::optional<std::string> artist)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.artist = std::move(artist); });
}

void track_impl::set_average_loudness(std::optional<double>)
{
    no_column("average loudness");
}

void track_impl::set_beatgrid(std::vector<beatgrid_marker>)
{
    no_column("beat grid");
}

void track_impl::set_bitrate(std::optional<int> bitrate)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.bitrate =
                bitrate ? std::make_optional<int64_t>(*bitrate) : std::nullopt;
        });
}

void track_impl::set_bpm(std::optional<double> bpm)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.bpm_x100 = bpm ? std::make_optional<int64_t>(
                                     static_cast<int64_t>(*bpm * 100))
                               : std::nullopt;
        });
}

void track_impl::set_comment(std::optional<std::string> comment)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.comment = std::move(comment); });
}

void track_impl::set_composer(std::optional<std::string> composer)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.composer = std::move(composer); });
}

void track_impl::set_duration(std::optional<std::chrono::milliseconds> duration)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.length =
                duration ? std::make_optional(
                               std::chrono::duration_cast<std::chrono::seconds>(
                                   *duration))
                         : std::nullopt;
        });
}

void track_impl::set_genre(std::optional<std::string> genre)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.genre = std::move(genre); });
}

void track_impl::set_hot_cue_at(int, std::optional<hot_cue>)
{
    no_column("hot cues");
}

void track_impl::set_hot_cues(std::vector<std::optional<hot_cue>>)
{
    no_column("hot cues");
}

void track_impl::set_key(std::optional<musical_key> key)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.key =
                key ? std::make_optional(to_key_name(*key)) : std::nullopt;
        });
}

void track_impl::set_last_played_at(
    std::optional<std::chrono::system_clock::time_point>)
{
    no_column("last-played time");
}

void track_impl::set_loop_at(int, std::optional<loop>)
{
    no_column("loops");
}

void track_impl::set_loops(std::vector<std::optional<loop>>)
{
    no_column("loops");
}

void track_impl::set_main_cue(std::optional<double>)
{
    no_column("main cue");
}

void track_impl::set_publisher(std::optional<std::string> publisher)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.label = std::move(publisher); });
}

void track_impl::set_rating(std::optional<int> rating)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.rating_stars = rating
                                   ? std::make_optional<int64_t>(*rating / 20)
                                   : std::nullopt;
        });
}

void track_impl::set_relative_path(std::string relative_path)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.path = to_device_path(relative_path); });
}

void track_impl::set_sample_count(std::optional<unsigned long long>)
{
    no_column("sample count");
}

void track_impl::set_sample_rate(std::optional<double> sample_rate)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.sampling_rate = sample_rate
                                    ? std::make_optional<int64_t>(
                                          static_cast<int64_t>(*sample_rate))
                                    : std::nullopt;
        });
}

void track_impl::set_title(std::optional<std::string> title)
{
    modify_content(
        context_, id(),
        [&](content_row& row) { row.title = std::move(title); });
}

void track_impl::set_track_number(std::optional<int> track_number)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.track_number = track_number
                                   ? std::make_optional<int64_t>(*track_number)
                                   : std::nullopt;
        });
}

void track_impl::set_waveform(std::vector<waveform_entry>)
{
    no_column("waveform");
}

void track_impl::set_year(std::optional<int> year)
{
    modify_content(
        context_, id(),
        [&](content_row& row)
        {
            row.release_year =
                year ? std::make_optional<int64_t>(*year) : std::nullopt;
        });
}

track create_track(
    const std::shared_ptr<onelibrary_context>& context,
    const track_snapshot& snapshot)
{
    const auto row = to_content_row(snapshot);
    return make_track(context, content_table{context}.add(row));
}

}  // namespace djinterop::onelibrary::v1
