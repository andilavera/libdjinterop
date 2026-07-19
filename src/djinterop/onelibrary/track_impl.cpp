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

#include <filesystem>
#include <stdexcept>

#include <djinterop/crate.hpp>
#include <djinterop/database.hpp>
#include <djinterop/exceptions.hpp>
#include <djinterop/onelibrary/onelibrary.hpp>
#include <djinterop/track.hpp>

#include "../util/filesystem.hpp"
#include "../util/sqlite_transaction.hpp"
#include <djinterop/onelibrary/album_table.hpp>
#include "anlz/anlz_writer.hpp"
#include "anlz/hash.hpp"
#include <djinterop/onelibrary/artist_table.hpp>
#include "content_table.hpp"
#include "database_impl.hpp"
#include "key_utils.hpp"
#include "onelibrary_context.hpp"
#include <djinterop/onelibrary/reference_tables.hpp>

namespace djinterop::onelibrary
{
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;

namespace
{

// Resolve FK id → string helpers.
std::string resolve_artist_name(
    sqlite::database& db, int64_t id)
{
    if (id <= 0) return {};
    std::string name;
    db << "SELECT name FROM artist WHERE artist_id = ?;" << id >> name;
    return name;
}

std::string resolve_album_name(
    sqlite::database& db, int64_t id)
{
    if (id <= 0) return {};
    std::string name;
    db << "SELECT name FROM album WHERE album_id = ?;" << id >> name;
    return name;
}

std::string resolve_genre_name(
    sqlite::database& db, int64_t id)
{
    if (id <= 0) return {};
    std::string name;
    db << "SELECT name FROM genre WHERE genre_id = ?;" << id >> name;
    return name;
}

std::string resolve_label_name(
    sqlite::database& db, int64_t id)
{
    if (id <= 0) return {};
    std::string name;
    db << "SELECT name FROM label WHERE label_id = ?;" << id >> name;
    return name;
}

std::optional<musical_key> resolve_key(
    sqlite::database& db, int64_t key_id)
{
    if (key_id <= 0) return std::nullopt;
    std::string name;
    db << "SELECT name FROM key WHERE key_id = ?;" << key_id >> name;
    if (name.empty()) return std::nullopt;
    return key_name_to_enum(name);
}

// Convert a content_row to a track_snapshot.
track_snapshot to_snapshot(
    sqlite::database& db, const content_row& row)
{
    track_snapshot s;

    s.title = row.title;
    s.artist = resolve_artist_name(db, row.artist_id);
    s.album = resolve_album_name(db, row.album_id);
    s.genre = resolve_genre_name(db, row.genre_id);
    s.comment = row.dj_comment;
    s.composer = resolve_artist_name(db, row.composer_id);
    s.publisher = resolve_label_name(db, row.label_id);
    s.relative_path = row.path;
    s.track_number = row.track_no > 0
                         ? std::make_optional(static_cast<int>(row.track_no))
                         : std::nullopt;
    s.duration = milliseconds{row.length * 1000};
    s.bpm = row.bpmx100 > 0
                ? std::make_optional(row.bpmx100 / 100.0)
                : std::nullopt;
    s.bitrate = row.bitrate > 0
                    ? std::make_optional(static_cast<int>(row.bitrate))
                    : std::nullopt;
    s.sample_rate = row.sampling_rate > 0
                        ? std::make_optional(static_cast<double>(row.sampling_rate))
                        : std::nullopt;
    s.sample_count =
        row.sampling_rate > 0 && row.length > 0
            ? std::make_optional(
                  static_cast<unsigned long long>(row.sampling_rate) *
                      static_cast<unsigned long long>(row.length))
            : std::nullopt;
    s.year = row.release_year
                 ? std::make_optional(static_cast<int>(*row.release_year))
                 : std::nullopt;
    s.rating = row.rating > 0 ? std::make_optional(row.rating * 20)
                              : std::nullopt;
    s.file_bytes =
        row.file_size > 0
            ? std::make_optional(static_cast<unsigned long long>(row.file_size))
            : std::nullopt;
    s.key = resolve_key(db, row.key_id);

    // ANLZ-only fields are returned empty.
    s.average_loudness = std::nullopt;
    s.beatgrid.clear();
    s.hot_cues.clear();
    s.loops.clear();
    s.waveform.clear();
    s.main_cue = std::nullopt;
    s.last_played_at = std::nullopt;

    return s;
}

// Create or lookup helpers using shared_ptr<onelibrary_context>.
int64_t resolve_or_create_artist(
    const std::shared_ptr<onelibrary_context>& ctx, const std::string& name)
{
    if (name.empty()) return 0;
    artist_table artists{ctx};
    return artists.add(name);
}

int64_t resolve_or_create_album(
    const std::shared_ptr<onelibrary_context>& ctx,
    const std::string& name, int64_t artist_id)
{
    if (name.empty()) return 0;
    album_table albums{ctx};
    return albums.add(name, artist_id);
}

int64_t resolve_or_create_genre(
    const std::shared_ptr<onelibrary_context>& ctx, const std::string& name)
{
    if (name.empty()) return 0;
    genre_table genres{ctx};
    return genres.add(name);
}

int64_t resolve_or_create_label(
    const std::shared_ptr<onelibrary_context>& ctx, const std::string& name)
{
    if (name.empty()) return 0;
    label_table labels{ctx};
    return labels.add(name);
}

// Convert a track_snapshot to the data needed to write ANLZ files.
anlz::anlz_track_data to_anlz_data(const track_snapshot& snapshot)
{
    return anlz::anlz_track_data{
        /* .relative_path = */ *snapshot.relative_path,
        /* .duration_secs = */ snapshot.duration
            ? static_cast<double>(
                  duration_cast<milliseconds>(*snapshot.duration)
                      .count()) /
                  1000.0
            : 0.0,
        /* .sample_rate = */ snapshot.sample_rate.value_or(44100.0),
        /* .average_loudness = */ snapshot.average_loudness,
        /* .beatgrid = */ snapshot.beatgrid,
        /* .hot_cues = */ snapshot.hot_cues,
        /* .loops = */ snapshot.loops,
        /* .waveform = */ snapshot.waveform,
        /* .main_cue = */ snapshot.main_cue,
    };
}

}  // anonymous namespace

// =============================================================================
// track_impl
// =============================================================================

track_impl::track_impl(
    std::shared_ptr<onelibrary_context> context, int64_t id) :
    djinterop::track_impl{id}, context_{std::move(context)}
{
}

track_snapshot track_impl::snapshot() const
{
    content_table ct{context_};
    auto row = ct.get(id());
    if (!row)
        throw djinterop::track_deleted{id()};
    return to_snapshot(context_->db, *row);
}

void track_impl::update(const track_snapshot& snapshot)
{
    if (!snapshot.relative_path)
    {
        throw invalid_track_snapshot{
            "Snapshot does not contain a populated `relative_path` field, "
            "which is required to update a track"};
    }

    util::sqlite_transaction trans{context_->db};

    auto artist_name = snapshot.artist.value_or("");
    auto artist_id = resolve_or_create_artist(context_, artist_name);

    auto album_name = snapshot.album.value_or("");
    int64_t album_id = 0;
    if (!album_name.empty())
        album_id = resolve_or_create_album(context_, album_name, artist_id);

    auto genre_name = snapshot.genre.value_or("");
    int64_t genre_id = 0;
    if (!genre_name.empty())
        genre_id = resolve_or_create_genre(context_, genre_name);

    auto label_name = snapshot.publisher.value_or("");
    int64_t label_id = 0;
    if (!label_name.empty())
        label_id = resolve_or_create_label(context_, label_name);

    int64_t composer_id = 0;
    auto composer_name = snapshot.composer.value_or("");
    if (!composer_name.empty())
        composer_id = resolve_or_create_artist(context_, composer_name);

    auto bpmx100 = snapshot.bpm
                       ? static_cast<int64_t>(*snapshot.bpm * 100)
                       : 0;
    auto length = snapshot.duration
                      ? static_cast<int64_t>(
                            duration_cast<seconds>(*snapshot.duration).count())
                      : 0;
    auto track_no = snapshot.track_number
                        ? static_cast<int64_t>(*snapshot.track_number)
                        : 0;
    auto bitrate = snapshot.bitrate
                       ? static_cast<int64_t>(*snapshot.bitrate)
                       : 0;
    auto sampling_rate = snapshot.sample_rate
                             ? static_cast<int64_t>(*snapshot.sample_rate)
                             : 0;
    auto file_size = snapshot.file_bytes
                         ? static_cast<int64_t>(*snapshot.file_bytes)
                         : 0;
    auto year = snapshot.year
                    ? std::make_optional(
                          static_cast<int64_t>(*snapshot.year))
                    : std::nullopt;
    auto rating = snapshot.rating
                      ? static_cast<int64_t>(*snapshot.rating / 20)
                      : 0;

    int64_t key_id = 0;
    if (snapshot.key)
    {
        auto* name = musical_key_to_name(*snapshot.key);
        if (name)
        {
            key_table keys{context_};
            key_id = keys.add(name);
        }
    }

    context_->db
        << "UPDATE content SET "
           "title = ?, artist_id_artist = ?, album_id = ?, genre_id = ?, "
           "label_id = ?, artist_id_composer = ?, key_id = ?, "
           "bpmx100 = ?, length = ?, "
           "trackNo = ?, bitrate = ?, samplingRate = ?, fileSize = ?, "
           "releaseYear = ?, rating = ?, path = ?, djComment = ? "
           "WHERE content_id = ?;"
        << (snapshot.title.value_or("")) << artist_id << album_id << genre_id
        << label_id << composer_id << key_id << bpmx100 << length
        << track_no << bitrate
        << sampling_rate << file_size << year << rating
        << *snapshot.relative_path
        << (snapshot.comment.value_or("")) << id();

    trans.commit();

    // Write ANLZ sidecar files with performance data from the snapshot.
    anlz::write_anlz_files(
        context_->directory, to_anlz_data(snapshot));
}

// Simple field getters/setters — most delegate to snapshot/update.

std::optional<std::string> track_impl::album()
{
    return snapshot().album;
}

void track_impl::set_album(std::optional<std::string> album)
{
    auto s = snapshot();
    s.album = std::move(album);
    update(s);
}

std::optional<std::string> track_impl::artist()
{
    return snapshot().artist;
}

void track_impl::set_artist(std::optional<std::string> artist)
{
    auto s = snapshot();
    s.artist = std::move(artist);
    update(s);
}

std::optional<double> track_impl::average_loudness()
{
    return snapshot().average_loudness;
}

void track_impl::set_average_loudness(std::optional<double> loudness)
{
    auto s = snapshot();
    s.average_loudness = std::move(loudness);
    update(s);
}

std::vector<beatgrid_marker> track_impl::beatgrid()
{
    return snapshot().beatgrid;
}

void track_impl::set_beatgrid(std::vector<beatgrid_marker> beatgrid)
{
    auto s = snapshot();
    s.beatgrid = std::move(beatgrid);
    update(s);
}

std::optional<int> track_impl::bitrate()
{
    return snapshot().bitrate;
}

void track_impl::set_bitrate(std::optional<int> bitrate)
{
    util::sqlite_transaction trans{context_->db};
    context_->db << "UPDATE content SET bitrate = ? WHERE content_id = ?;"
                 << (bitrate.value_or(0)) << id();
    trans.commit();
}

std::optional<double> track_impl::bpm()
{
    return snapshot().bpm;
}

void track_impl::set_bpm(std::optional<double> bpm)
{
    util::sqlite_transaction trans{context_->db};
    auto bpmx100 = bpm ? static_cast<int64_t>(*bpm * 100) : 0;
    context_->db << "UPDATE content SET bpmx100 = ? WHERE content_id = ?;"
                 << bpmx100 << id();
    trans.commit();
}

std::optional<std::string> track_impl::comment()
{
    return snapshot().comment;
}

void track_impl::set_comment(std::optional<std::string> comment)
{
    util::sqlite_transaction trans{context_->db};
    context_->db << "UPDATE content SET djComment = ? WHERE content_id = ?;"
                 << comment << id();
    trans.commit();
}

std::optional<std::string> track_impl::composer()
{
    return snapshot().composer;
}

void track_impl::set_composer(std::optional<std::string> composer)
{
    auto s = snapshot();
    s.composer = std::move(composer);
    update(s);
}

std::vector<crate> track_impl::containing_crates()
{
    return {};
}

database track_impl::db()
{
    return database{
        std::make_shared<database_impl>(context_)};
}

std::optional<milliseconds> track_impl::duration()
{
    return snapshot().duration;
}

void track_impl::set_duration(std::optional<milliseconds> duration)
{
    util::sqlite_transaction trans{context_->db};
    auto secs = duration
                    ? static_cast<int64_t>(
                          duration_cast<seconds>(*duration).count())
                    : 0;
    context_->db << "UPDATE content SET length = ? WHERE content_id = ?;"
                 << secs << id();
    trans.commit();
}

std::string track_impl::file_extension()
{
    auto rp = relative_path();
    return djinterop::util::get_file_extension(rp).value_or(std::string{});
}

std::string track_impl::filename()
{
    auto rp = relative_path();
    return djinterop::util::get_filename(rp);
}

std::optional<std::string> track_impl::genre()
{
    return snapshot().genre;
}

void track_impl::set_genre(std::optional<std::string> genre)
{
    auto s = snapshot();
    s.genre = std::move(genre);
    update(s);
}

std::optional<hot_cue> track_impl::hot_cue_at(int index)
{
    auto cues = snapshot().hot_cues;
    if (index < 0 || static_cast<size_t>(index) >= cues.size())
        return std::nullopt;
    return cues[static_cast<size_t>(index)];
}

void track_impl::set_hot_cue_at(int index, std::optional<hot_cue> cue)
{
    if (index < 0) return;
    auto s = snapshot();
    auto idx = static_cast<size_t>(index);
    if (idx >= s.hot_cues.size())
        s.hot_cues.resize(idx + 1);
    s.hot_cues[idx] = std::move(cue);
    update(s);
}

std::vector<std::optional<hot_cue>> track_impl::hot_cues()
{
    return snapshot().hot_cues;
}

void track_impl::set_hot_cues(std::vector<std::optional<hot_cue>> cues)
{
    auto s = snapshot();
    s.hot_cues = std::move(cues);
    update(s);
}

bool track_impl::is_valid()
{
    content_table ct{context_};
    return ct.get(id()).has_value();
}

std::optional<musical_key> track_impl::key()
{
    return snapshot().key;
}

void track_impl::set_key(std::optional<musical_key> key)
{
    util::sqlite_transaction trans{context_->db};
    int64_t key_id = 0;
    if (key)
    {
        auto* name = musical_key_to_name(*key);
        if (name)
        {
            key_table keys{context_};
            key_id = keys.add(name);
        }
    }
    context_->db << "UPDATE content SET key_id = ? WHERE content_id = ?;"
                 << key_id << id();
    trans.commit();
}

std::optional<std::chrono::system_clock::time_point>
track_impl::last_played_at()
{
    return snapshot().last_played_at;
}

void track_impl::set_last_played_at(
    std::optional<std::chrono::system_clock::time_point> time)
{
    auto s = snapshot();
    s.last_played_at = std::move(time);
    update(s);
}

std::optional<loop> track_impl::loop_at(int index)
{
    auto lps = snapshot().loops;
    if (index < 0 || static_cast<size_t>(index) >= lps.size())
        return std::nullopt;
    return lps[static_cast<size_t>(index)];
}

void track_impl::set_loop_at(int index, std::optional<loop> l)
{
    if (index < 0) return;
    auto s = snapshot();
    auto idx = static_cast<size_t>(index);
    if (idx >= s.loops.size())
        s.loops.resize(idx + 1);
    s.loops[idx] = std::move(l);
    update(s);
}

std::vector<std::optional<loop>> track_impl::loops()
{
    return snapshot().loops;
}

void track_impl::set_loops(std::vector<std::optional<loop>> loops)
{
    auto s = snapshot();
    s.loops = std::move(loops);
    update(s);
}

std::optional<double> track_impl::main_cue()
{
    return snapshot().main_cue;
}

void track_impl::set_main_cue(std::optional<double> sample_offset)
{
    auto s = snapshot();
    s.main_cue = std::move(sample_offset);
    update(s);
}

std::optional<std::string> track_impl::publisher()
{
    return snapshot().publisher;
}

void track_impl::set_publisher(std::optional<std::string> publisher)
{
    auto s = snapshot();
    s.publisher = std::move(publisher);
    update(s);
}

std::optional<int32_t> track_impl::rating()
{
    return snapshot().rating;
}

void track_impl::set_rating(std::optional<int32_t> rating)
{
    util::sqlite_transaction trans{context_->db};
    auto r = rating ? static_cast<int64_t>(*rating / 20) : 0;
    context_->db << "UPDATE content SET rating = ? WHERE content_id = ?;"
                 << r << id();
    trans.commit();
}

std::string track_impl::relative_path()
{
    std::string path;
    context_->db
        << "SELECT path FROM content WHERE content_id = ?;" << id() >> path;
    return path;
}

void track_impl::set_relative_path(std::string relative_path)
{
    util::sqlite_transaction trans{context_->db};
    context_->db
        << "UPDATE content SET path = ? WHERE content_id = ?;"
        << relative_path << id();
    trans.commit();
}

std::optional<unsigned long long> track_impl::sample_count()
{
    return snapshot().sample_count;
}

void track_impl::set_sample_count(
    std::optional<unsigned long long> sample_count)
{
    auto s = snapshot();
    s.sample_count = std::move(sample_count);
    update(s);
}

std::optional<double> track_impl::sample_rate()
{
    return snapshot().sample_rate;
}

void track_impl::set_sample_rate(std::optional<double> sample_rate)
{
    util::sqlite_transaction trans{context_->db};
    auto sr = sample_rate ? static_cast<int64_t>(*sample_rate) : 0;
    context_->db
        << "UPDATE content SET samplingRate = ? WHERE content_id = ?;"
        << sr << id();
    trans.commit();
}

std::optional<std::string> track_impl::title()
{
    return snapshot().title;
}

void track_impl::set_title(std::optional<std::string> title)
{
    util::sqlite_transaction trans{context_->db};
    context_->db << "UPDATE content SET title = ? WHERE content_id = ?;"
                 << (title.value_or("")) << id();
    trans.commit();
}

std::optional<int> track_impl::track_number()
{
    return snapshot().track_number;
}

void track_impl::set_track_number(std::optional<int> track_number)
{
    util::sqlite_transaction trans{context_->db};
    auto tn = track_number ? static_cast<int64_t>(*track_number) : 0;
    context_->db << "UPDATE content SET trackNo = ? WHERE content_id = ?;"
                 << tn << id();
    trans.commit();
}

std::vector<waveform_entry> track_impl::waveform()
{
    return snapshot().waveform;
}

void track_impl::set_waveform(std::vector<waveform_entry> waveform)
{
    auto s = snapshot();
    s.waveform = std::move(waveform);
    update(s);
}

std::optional<int> track_impl::year()
{
    return snapshot().year;
}

void track_impl::set_year(std::optional<int> year)
{
    util::sqlite_transaction trans{context_->db};
    std::optional<int64_t> y;
    if (year) y = static_cast<int64_t>(*year);
    context_->db
        << "UPDATE content SET releaseYear = ? WHERE content_id = ?;"
        << y << id();
    trans.commit();
}

// =============================================================================
// create_track (free function)
// =============================================================================

track create_track_from_snapshot(
    const std::shared_ptr<onelibrary_context>& context,
    const track_snapshot& snapshot)
{
    track_info info;
    info.title = snapshot.title.value_or("");
    info.artist = snapshot.artist.value_or("");
    info.album = snapshot.album;
    info.genre = snapshot.genre;
    info.label = snapshot.publisher;
    info.composer = snapshot.composer;
    info.relative_path =
        snapshot.relative_path.value_or("");
    info.track_number =
        snapshot.track_number.value_or(0);
    info.bitrate = snapshot.bitrate.value_or(0);
    info.bpm = snapshot.bpm.value_or(0);
    info.sample_rate =
        snapshot.sample_rate
            ? static_cast<int64_t>(*snapshot.sample_rate)
            : 0;
    info.file_size_bytes =
        snapshot.file_bytes
            ? static_cast<int64_t>(*snapshot.file_bytes)
            : 0;
    info.duration_secs =
        snapshot.duration
            ? static_cast<int64_t>(
                  duration_cast<seconds>(*snapshot.duration).count())
            : 0;
    info.year = snapshot.year
                    ? std::make_optional(
                          static_cast<int64_t>(*snapshot.year))
                    : std::nullopt;
    info.rating = snapshot.rating
                      ? static_cast<int64_t>(*snapshot.rating / 20)
                      : 0;

    // Work directly with the tables to add the track.
    artist_table artists{context};
    auto artist_id = artists.add(info.artist);

    int64_t album_artist_id = 0;
    if (info.album_artist && !info.album_artist->empty())
        album_artist_id = artists.add(*info.album_artist);
    else
        album_artist_id = artist_id;

    int64_t album_id = 0;
    if (info.album && !info.album->empty())
    {
        album_table albums{context};
        album_id = albums.add(*info.album, album_artist_id);
    }

    int64_t genre_id = 0;
    if (info.genre && !info.genre->empty())
    {
        genre_table genres{context};
        genre_id = genres.add(*info.genre);
    }

    int64_t label_id = 0;
    if (info.label && !info.label->empty())
    {
        label_table labels{context};
        label_id = labels.add(*info.label);
    }

    int64_t key_id = 0;
    if (info.key && !info.key->empty())
    {
        key_table keys{context};
        key_id = keys.add(*info.key);
    }

    int64_t composer_id = 0;
    if (info.composer && !info.composer->empty())
        composer_id = artists.add(*info.composer);

    int64_t lyricist_id = 0;
    if (info.lyricist && !info.lyricist->empty())
        lyricist_id = artists.add(*info.lyricist);

    int64_t remixer_id = 0;
    if (info.remixer && !info.remixer->empty())
        remixer_id = artists.add(*info.remixer);

    std::string file_name =
        std::filesystem::path{info.relative_path}.filename().string();

    content_row row;
    row.title = info.title;
    row.bpmx100 = static_cast<int64_t>(info.bpm * 100);
    row.length = info.duration_secs;
    row.track_no = info.track_number;
    row.disc_no = info.disc_number;
    row.artist_id = artist_id;
    row.remixer_id = remixer_id;
    row.original_artist_id = 0;
    row.composer_id = composer_id;
    row.lyricist_id = lyricist_id;
    row.album_id = album_id;
    row.genre_id = genre_id;
    row.label_id = label_id;
    row.key_id = key_id;
    row.rating = info.rating;
    row.image_id = info.image_id;
    row.release_year = info.year;
    row.path = info.relative_path;
    row.file_name = file_name;
    row.file_size = info.file_size_bytes;
    row.file_type = info.file_type;
    row.bitrate = info.bitrate;
    row.bit_depth = info.bit_depth;
    row.sampling_rate = info.sample_rate;
    row.isrc = info.isrc;

    // Set analysis data file path.
    auto anlz = anlz::compute_anlz_path(info.relative_path);
    row.analysis_data_file_path =
        "/.PIONEER/USBANLZ/" + anlz.to_directory() + "/ANLZ0000.DAT";

    // Set dates.
    auto now_str = [] {
        char buf[20];
        time_t now = time(nullptr);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return std::string{buf};
    };
    row.date_added = now_str();

    content_table ct{context};
    auto id = ct.add(row);

    // Write ANLZ sidecar files for the new track.
    anlz::write_anlz_files(
        context->directory, to_anlz_data(snapshot));

    return track{std::make_shared<track_impl>(context, id)};
}

}  // namespace djinterop::onelibrary
