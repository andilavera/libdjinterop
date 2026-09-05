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

#include <djinterop/onelibrary/onelibrary.hpp>

#include <filesystem>
#include <stdexcept>

#include <sqlite_modern_cpp.h>

#include <djinterop/onelibrary/artist_table.hpp>
#include <djinterop/onelibrary/album_table.hpp>
#include <djinterop/onelibrary/reference_tables.hpp>
#include "content_table.hpp"
#include "onelibrary_context.hpp"
#include "schema.hpp"
#include "../util/sqlite_transaction.hpp"

namespace djinterop::onelibrary
{

namespace
{
namespace fs = std::filesystem;

constexpr const char* k_db_rel_path = ".PIONEER/rekordbox/exportLibrary.db";

/// Open a SQLCipher database, setting key and compatibility mode.
sqlite::database open_db(const std::string& path, bool create)
{
    if (create)
    {
        // Ensure parent directories exist.
        fs::create_directories(fs::path{path}.parent_path());
    }

    sqlite::database db{path};

    // Set key via PRAGMA (SQLCipher passphrase mode).
    db << "PRAGMA key = '" + std::string{default_key} + "';";
    db << "PRAGMA cipher_compatibility = 4;";

    return db;
}
}  // anonymous namespace

onelibrary onelibrary::create(const std::string& directory)
{
    if (exists(directory))
    {
        throw std::runtime_error{
            "OneLibrary database already exists at " + directory};
    }

    auto db_path = fs::path{directory} / k_db_rel_path;
    auto db = open_db(db_path.string(), /*create=*/true);

    // Create schema on a brand-new encrypted database.
    create_tables(db.connection().get());
    insert_default_catalogues(db.connection().get());

    // Checkpoint the WAL.
    db << "PRAGMA wal_checkpoint(TRUNCATE);";

    auto ctx = std::make_shared<onelibrary_context>(directory, std::move(db));
    return onelibrary{ctx};
}

onelibrary onelibrary::load(const std::string& directory)
{
    if (!exists(directory))
    {
        throw std::runtime_error{
            "OneLibrary database not found at " + directory};
    }

    auto db_path = fs::path{directory} / k_db_rel_path;
    auto db = open_db(db_path.string(), /*create=*/false);

    // Verify we can read it.
    int count = 0;
    db << "SELECT count(*) FROM sqlite_master;" >> count;
    if (count == 0)
    {
        throw std::runtime_error{
            "Failed to decrypt or read OneLibrary database at " +
            db_path.string()};
    }

    auto ctx = std::make_shared<onelibrary_context>(directory, std::move(db));
    return onelibrary{ctx};
}

bool onelibrary::exists(const std::string& directory)
{
    return fs::exists(fs::path{directory} / k_db_rel_path);
}

onelibrary::onelibrary(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)},
    artist_{std::make_unique<artist_table>(context_)},
    album_{std::make_unique<album_table>(context_)},
    genre_{std::make_unique<genre_table>(context_)},
    label_{std::make_unique<label_table>(context_)},
    key_{std::make_unique<key_table>(context_)},
    content_{std::make_unique<content_table>(context_)}
{
}

onelibrary::~onelibrary() = default;

int64_t onelibrary::add_track(const track_info& track)
{
    util::sqlite_transaction trans{context_->db};

    // Resolve or create reference-table rows.
    auto artist_id = artist_->add(track.artist);

    int64_t album_artist_id = 0;
    if (track.album_artist && !track.album_artist->empty())
        album_artist_id = artist_->add(*track.album_artist);
    else
        album_artist_id = artist_id;

    int64_t album_id = 0;
    if (track.album && !track.album->empty())
        album_id = album_->add(*track.album, album_artist_id);

    int64_t genre_id = 0;
    if (track.genre && !track.genre->empty())
        genre_id = genre_->add(*track.genre);

    int64_t label_id = 0;
    if (track.label && !track.label->empty())
        label_id = label_->add(*track.label);

    int64_t key_id = 0;
    if (track.key && !track.key->empty())
        key_id = key_->add(*track.key);

    int64_t composer_id = 0;
    if (track.composer && !track.composer->empty())
        composer_id = artist_->add(*track.composer);

    int64_t lyricist_id = 0;
    if (track.lyricist && !track.lyricist->empty())
        lyricist_id = artist_->add(*track.lyricist);

    int64_t remixer_id = 0;
    if (track.remixer && !track.remixer->empty())
        remixer_id = artist_->add(*track.remixer);

    // Extract filename from relative path.
    std::string file_name =
        fs::path{track.relative_path}.filename().string();

    // Build the content row.
    content_row row;
    row.title = track.title;
    row.bpmx100 = static_cast<int64_t>(track.bpm * 100);
    row.length = track.duration_secs;
    row.track_no = track.track_number;
    row.disc_no = track.disc_number;
    row.artist_id = artist_id;
    row.remixer_id = remixer_id;
    row.original_artist_id = 0;
    row.composer_id = composer_id;
    row.lyricist_id = lyricist_id;
    row.album_id = album_id;
    row.genre_id = genre_id;
    row.label_id = label_id;
    row.key_id = key_id;
    row.rating = track.rating;
    row.release_year = track.year;
    row.path = track.relative_path;
    row.file_name = file_name;
    row.file_size = track.file_size_bytes;
    row.file_type = track.file_type;
    row.bitrate = track.bitrate;
    row.bit_depth = track.bit_depth;
    row.sampling_rate = track.sample_rate;
    row.isrc = track.isrc;

    // Set dates to now.
    auto now_str = [] {
        char buf[20];
        time_t now = time(nullptr);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return std::string{buf};
    };
    row.date_added = now_str();

    auto content_id = content_->add(row);
    trans.commit();
    return content_id;
}

std::optional<track_info> onelibrary::get_track(int64_t id) const
{
    auto row = content_->get(id);
    if (!row) return std::nullopt;

    track_info info;
    info.title = row->title;
    info.relative_path = row->path;
    info.bpm = row->bpmx100 / 100.0;
    info.duration_secs = row->length;
    info.track_number = row->track_no;
    info.disc_number = row->disc_no;
    info.bitrate = row->bitrate;
    info.bit_depth = row->bit_depth;
    info.sample_rate = row->sampling_rate;
    info.file_size_bytes = row->file_size;
    info.file_type = row->file_type;
    info.isrc = row->isrc;
    info.rating = row->rating;
    info.year = row->release_year;

    // Resolve FK names from reference tables.
    auto& db = context_->db;

    if (row->artist_id > 0)
    {
        std::string name;
        db << "SELECT name FROM artist WHERE artist_id = ?;"
           << row->artist_id >> name;
        info.artist = std::move(name);
    }

    if (row->album_id > 0)
    {
        std::string name;
        db << "SELECT name FROM album WHERE album_id = ?;"
           << row->album_id >> name;
        info.album = std::move(name);
    }

    if (row->genre_id > 0)
    {
        std::string name;
        db << "SELECT name FROM genre WHERE genre_id = ?;"
           << row->genre_id >> name;
        info.genre = std::move(name);
    }

    if (row->label_id > 0)
    {
        std::string name;
        db << "SELECT name FROM label WHERE label_id = ?;"
           << row->label_id >> name;
        info.label = std::move(name);
    }

    if (row->key_id > 0)
    {
        std::string name;
        db << "SELECT name FROM key WHERE key_id = ?;"
           << row->key_id >> name;
        info.key = std::move(name);
    }

    if (row->composer_id > 0)
    {
        std::string name;
        db << "SELECT name FROM artist WHERE artist_id = ?;"
           << row->composer_id >> name;
        info.composer = std::move(name);
    }

    if (row->lyricist_id > 0)
    {
        std::string name;
        db << "SELECT name FROM artist WHERE artist_id = ?;"
           << row->lyricist_id >> name;
        info.lyricist = std::move(name);
    }

    if (row->remixer_id > 0)
    {
        std::string name;
        db << "SELECT name FROM artist WHERE artist_id = ?;"
           << row->remixer_id >> name;
        info.remixer = std::move(name);
    }

    return info;
}

std::vector<int64_t> onelibrary::track_ids() const
{
    return content_->all_ids();
}

void onelibrary::remove_track(int64_t id)
{
    util::sqlite_transaction trans{context_->db};
    context_->db << "DELETE FROM content WHERE content_id = ?;" << id;
    trans.commit();
}

std::optional<track_info> onelibrary::get_track_by_relative_path(
    const std::string& relative_path) const
{
    int64_t id = 0;
    context_->db
        << "SELECT COALESCE("
           "(SELECT content_id FROM content WHERE path = ?), 0);"
        << relative_path >> id;
    if (id == 0) return std::nullopt;
    return get_track(id);
}

std::string onelibrary::directory() const
{
    return context_->directory;
}

int64_t onelibrary::track_count() const
{
    int64_t count = 0;
    context_->db << "SELECT count(*) FROM content;" >> count;
    return count;
}

void onelibrary::verify() const
{
    std::string result;
    context_->db << "PRAGMA integrity_check;" >> result;
    if (result != "ok")
    {
        throw std::runtime_error{"Database integrity check failed: " + result};
    }
}

}  // namespace djinterop::onelibrary
