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

#define BOOST_TEST_MODULE onelibrary_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <sqlite3.h>

#include <djinterop/djinterop.hpp>

#include "../temporary_directory.hpp"

#define STRINGIFY(x) STRINGIFY_(x)
#define STRINGIFY_(x) #x

namespace utf = boost::unit_test;
namespace fs = std::filesystem;
namespace onelib = djinterop::onelibrary;

namespace
{
constexpr const char* k_key = onelib::default_key;
constexpr const char* k_db_rel_path = ".PIONEER/rekordbox/exportLibrary.db";
constexpr const char* k_reference_fixture =
    STRINGIFY(TESTDATA_DIR)
    "/ref/onelibrary/rekordbox-7.0/exportLibrary.db.sql";

// Applies the SQLCipher key and compatibility mode to an open connection.
void apply_key(sqlite3* db)
{
    char* err = nullptr;
    std::string pragma_key = std::string{"PRAGMA key = '"} + k_key + "';";
    int rc = sqlite3_exec(db, pragma_key.c_str(), nullptr, nullptr, &err);
    BOOST_REQUIRE_MESSAGE(
        rc == SQLITE_OK, "PRAGMA key failed: " << (err ? err : ""));
    if (err) sqlite3_free(err);

    rc = sqlite3_exec(
        db, "PRAGMA cipher_compatibility = 4;", nullptr, nullptr, &err);
    BOOST_REQUIRE_MESSAGE(
        rc == SQLITE_OK,
        "PRAGMA cipher_compatibility failed: " << (err ? err : ""));
    if (err) sqlite3_free(err);
}

sqlite3* open_db_file(const std::string& path, bool create)
{
    sqlite3* db;
    int flags =
        create ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
               : SQLITE_OPEN_READWRITE;
    int rc = sqlite3_open_v2(path.c_str(), &db, flags, nullptr);
    BOOST_REQUIRE_EQUAL(rc, SQLITE_OK);
    apply_key(db);
    return db;
}

sqlite3* open_for_verify(const std::string& dir)
{
    return open_db_file((fs::path{dir} / k_db_rel_path).string(), false);
}

// Reads a whole file into a string.
std::string read_file(const std::string& path)
{
    std::ifstream in{path, std::ios::binary};
    BOOST_REQUIRE_MESSAGE(in.is_open(), "Cannot open " << path);
    return std::string{
        std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void close_for_verify(sqlite3* db);

// Hydrates a new SQLCipher database file from a plain SQL script.
void hydrate_db_file(const std::string& path, const std::string& script)
{
    sqlite3* db = open_db_file(path, /*create=*/true);
    char* err = nullptr;
    int rc = sqlite3_exec(db, script.c_str(), nullptr, nullptr, &err);
    BOOST_REQUIRE_MESSAGE(
        rc == SQLITE_OK, "Script execution failed: " << (err ? err : ""));
    if (err) sqlite3_free(err);
    close_for_verify(db);
}

// Fetches all rows of a query as string cells.
std::vector<std::vector<std::string>> fetch_rows(
    sqlite3* db, const std::string& sql)
{
    std::vector<std::vector<std::string>> rows;
    sqlite3_stmt* stmt = nullptr;
    BOOST_REQUIRE_EQUAL(
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr), SQLITE_OK);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::vector<std::string> row;
        for (int i = 0; i < sqlite3_column_count(stmt); ++i)
        {
            const char* text =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.emplace_back(text ? text : "");
        }
        rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return rows;
}

int query_int(sqlite3* db, const char* sql)
{
    int result = -1;
    auto cb = [](void* data, int, char** values, char**) -> int {
        *static_cast<int*>(data) = std::stoi(values[0]);
        return 0;
    };
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, cb, &result, &err);
    BOOST_REQUIRE_MESSAGE(
        rc == SQLITE_OK,
        "Query failed: " << sql << " — " << (err ? err : ""));
    if (err) sqlite3_free(err);
    return result;
}

std::string query_text(sqlite3* db, const char* sql)
{
    std::string result;
    auto cb = [](void* data, int, char** values, char**) -> int {
        *static_cast<std::string*>(data) = values[0] ? values[0] : "";
        return 0;
    };
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, cb, &result, &err);
    BOOST_REQUIRE_MESSAGE(
        rc == SQLITE_OK,
        "Query failed: " << sql << " — " << (err ? err : ""));
    if (err) sqlite3_free(err);
    return result;
}

void close_for_verify(sqlite3* db) { sqlite3_close(db); }

}  // anonymous namespace

BOOST_TEST_DECORATOR(
    *utf::description("create() produces a valid empty export"))
BOOST_AUTO_TEST_CASE(create__empty__produces_valid_export)
{
    temporary_directory tmp_loc;

    {
        auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);
        BOOST_CHECK_NO_THROW(lib.verify());
        BOOST_CHECK_EQUAL(lib.track_count(), 0);
    }

    auto db_path = fs::path{tmp_loc.temp_dir} / k_db_rel_path;
    BOOST_TEST(fs::exists(db_path));
    BOOST_TEST(fs::file_size(db_path) > 0u);

    auto wal_path = fs::path{tmp_loc.temp_dir} /
                    ".PIONEER/rekordbox/exportLibrary.db-wal";
    BOOST_TEST(!fs::exists(wal_path));

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);

    BOOST_TEST(query_int(db, "SELECT count(*) FROM sqlite_master WHERE type='table'") == 22);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM sqlite_master WHERE type='index'") == 4);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM color") == 8);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM menuItem") == 27);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM category") == 22);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM sort") == 17);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM myTag") == 28);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM property") == 1);
    BOOST_TEST(query_text(db, "SELECT dbVersion FROM property") == "1000");
    BOOST_TEST(query_text(db, "SELECT deviceName FROM property") == "");
    BOOST_TEST(
        query_int(
            db, "SELECT count(*) FROM property WHERE myTagMasterDBID != 0") ==
        1);

    const std::vector<const char*> empty_tables{
        "content",          "artist",        "album",
        "genre",            "label",         "key",
        "image",            "cue",           "playlist",
        "playlist_content", "history",       "history_content",
        "hotCueBankList",   "hotCueBankList_cue",
        "myTag_content",    "recommendedLike"};
    for (auto* tbl : empty_tables)
    {
        std::string sql = std::string{"SELECT count(*) FROM "} + tbl;
        BOOST_TEST(query_int(db, sql.c_str()) == 0,
                   "Table " << tbl << " should be empty");
    }

    char* err = nullptr;
    int rc = sqlite3_exec(db, "PRAGMA integrity_check;", nullptr, nullptr, &err);
    BOOST_TEST(rc == SQLITE_OK);
    BOOST_TEST(err == nullptr);
    if (err) sqlite3_free(err);

    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description("create() throws if database already exists"))
BOOST_AUTO_TEST_CASE(create__already_exists__throws)
{
    temporary_directory tmp_loc;
    onelib::onelibrary::create(tmp_loc.temp_dir);
    BOOST_CHECK_THROW(
        onelib::onelibrary::create(tmp_loc.temp_dir), std::runtime_error);
}

BOOST_TEST_DECORATOR(
    *utf::description("exists() returns correct values"))
BOOST_AUTO_TEST_CASE(exists__empty_dir__returns_false)
{
    temporary_directory tmp_loc;
    BOOST_TEST(!onelib::onelibrary::exists(tmp_loc.temp_dir));
    onelib::onelibrary::create(tmp_loc.temp_dir);
    BOOST_TEST(onelib::onelibrary::exists(tmp_loc.temp_dir));
}

BOOST_TEST_DECORATOR(
    *utf::description("create() with nested path creates dirs"))
BOOST_AUTO_TEST_CASE(create__nested_path__creates_dirs)
{
    temporary_directory tmp_loc;
    auto nested = tmp_loc.temp_dir + "/sub1/sub2/sub3";
    auto lib = onelib::onelibrary::create(nested);
    BOOST_TEST(onelib::onelibrary::exists(nested));
    lib.verify();
}

BOOST_TEST_DECORATOR(
    *utf::description(
        "rekordbox reference fixture hydrates into a valid database"))
BOOST_AUTO_TEST_CASE(reference_fixture__hydrates__valid)
{
    temporary_directory tmp_loc;
    auto ref_path = fs::path{tmp_loc.temp_dir} / "reference.db";
    hydrate_db_file(ref_path.string(), read_file(k_reference_fixture));

    sqlite3* db = open_db_file(ref_path.string(), /*create=*/false);

    BOOST_TEST(query_int(db, "SELECT count(*) FROM sqlite_master WHERE type='table'") == 22);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM sqlite_master WHERE type='index'") == 4);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM color") == 8);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM menuItem") == 27);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM category") == 22);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM sort") == 17);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM myTag") == 28);
    BOOST_TEST(query_text(db, "SELECT dbVersion FROM property") == "1000");

    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description(
        "create() seed tables match the rekordbox reference fixture"))
BOOST_AUTO_TEST_CASE(create__seed_tables__match_reference_fixture)
{
    temporary_directory tmp_loc;

    onelib::onelibrary::create(tmp_loc.temp_dir);

    auto ref_path = fs::path{tmp_loc.temp_dir} / "reference.db";
    hydrate_db_file(ref_path.string(), read_file(k_reference_fixture));

    sqlite3* created = open_db_file(
        (fs::path{tmp_loc.temp_dir} / k_db_rel_path).string(), /*create=*/false);
    sqlite3* ref = open_db_file(ref_path.string(), /*create=*/false);

    const char* seed_tables[] = {
        "color", "menuItem", "category", "sort", "myTag"};
    for (auto* tbl : seed_tables)
    {
        BOOST_TEST_CONTEXT("Seed table " << tbl)
        {
            auto created_rows = fetch_rows(
                created,
                std::string{"SELECT * FROM "} + tbl + " ORDER BY 1");
            auto ref_rows = fetch_rows(
                ref, std::string{"SELECT * FROM "} + tbl + " ORDER BY 1");
            BOOST_REQUIRE_EQUAL(created_rows.size(), ref_rows.size());
            for (size_t i = 0; i < created_rows.size(); ++i)
            {
                BOOST_REQUIRE_EQUAL(
                    created_rows[i].size(), ref_rows[i].size());
                for (size_t j = 0; j < created_rows[i].size(); ++j)
                    BOOST_CHECK_EQUAL(created_rows[i][j], ref_rows[i][j]);
            }
        }
    }

    close_for_verify(created);
    close_for_verify(ref);
}

BOOST_TEST_DECORATOR(
    *utf::description("artist_table add() returns a new id"))
BOOST_AUTO_TEST_CASE(artist__add__returns_id)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id = lib.artist().add("Test Artist");
    BOOST_TEST(id > 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("artist_table add() deduplicates by name"))
BOOST_AUTO_TEST_CASE(artist__add__same_name__deduplicates)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id1 = lib.artist().add("Same Artist");
    auto id2 = lib.artist().add("Same Artist");
    BOOST_TEST(id1 == id2);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM artist") == 1);
    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description("artist_table find_id() round-trips added artists"))
BOOST_AUTO_TEST_CASE(artist__find_id__round_trips)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id = lib.artist().add("Find Me");
    BOOST_TEST(lib.artist().find_id("Find Me").value() == id);
    BOOST_TEST(!lib.artist().find_id("Missing").has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("album_table add() returns a new id"))
BOOST_AUTO_TEST_CASE(album__add__returns_id)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id = lib.album().add("Test Album");
    BOOST_TEST(id > 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("album_table add() deduplicates by name"))
BOOST_AUTO_TEST_CASE(album__add__same_name__deduplicates)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id1 = lib.album().add("Same Album");
    auto id2 = lib.album().add("Same Album");
    BOOST_TEST(id1 == id2);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM album") == 1);
    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description("album_table add() persists artist reference"))
BOOST_AUTO_TEST_CASE(album__add__with_artist__persists)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto artist_id = lib.artist().add("Album Artist");
    lib.album().add("Album By Artist", artist_id);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT artist_id FROM album") == artist_id);
    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description("album_table find_id() round-trips added albums"))
BOOST_AUTO_TEST_CASE(album__find_id__round_trips)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id = lib.album().add("Find Me");
    BOOST_TEST(lib.album().find_id("Find Me").value() == id);
    BOOST_TEST(!lib.album().find_id("Missing").has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("genre_table add() deduplicates and find_id() round-trips"))
BOOST_AUTO_TEST_CASE(genre__add__deduplicates_and_finds)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id1 = lib.genre().add("Techno");
    auto id2 = lib.genre().add("Techno");
    BOOST_TEST(id1 > 0);
    BOOST_TEST(id1 == id2);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM genre") == 1);
    close_for_verify(db);

    BOOST_TEST(lib.genre().find_id("Techno").value() == id1);
    BOOST_TEST(!lib.genre().find_id("Missing").has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("label_table add() deduplicates and find_id() round-trips"))
BOOST_AUTO_TEST_CASE(label__add__deduplicates_and_finds)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id1 = lib.label().add("Test Label");
    auto id2 = lib.label().add("Test Label");
    BOOST_TEST(id1 > 0);
    BOOST_TEST(id1 == id2);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM label") == 1);
    close_for_verify(db);

    BOOST_TEST(lib.label().find_id("Test Label").value() == id1);
    BOOST_TEST(!lib.label().find_id("Missing").has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("key_table add() deduplicates and find_id() round-trips"))
BOOST_AUTO_TEST_CASE(key__add__deduplicates_and_finds)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    auto id1 = lib.key().add("Am");
    auto id2 = lib.key().add("Am");
    BOOST_TEST(id1 > 0);
    BOOST_TEST(id1 == id2);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM key") == 1);
    close_for_verify(db);

    BOOST_TEST(lib.key().find_id("Am").value() == id1);
    BOOST_TEST(!lib.key().find_id("Missing").has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("add_track() adds a track with resolved references"))
BOOST_AUTO_TEST_CASE(add_track__valid__adds_content)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    onelib::track_info track;
    track.title = "Test Track";
    track.artist = "Test Artist";
    track.album = "Test Album";
    track.genre = "Test Genre";
    track.bpm = 128.5;
    track.duration_secs = 300;
    track.file_type = 5;  // FLAC
    track.bitrate = 1024;
    track.sample_rate = 44100;
    track.relative_path = "/Contents/Test Track.flac";

    int64_t id = lib.add_track(track);
    BOOST_TEST(id > 0);
    BOOST_TEST(lib.track_count() == 1);

    // Verify via raw SQL.
    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM content") == 1);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM artist") == 1);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM album") == 1);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM genre") == 1);
    BOOST_TEST(query_text(db, "SELECT title FROM content WHERE content_id = 1") == "Test Track");
    BOOST_TEST(query_text(db, "SELECT name FROM artist WHERE artist_id = 1") == "Test Artist");
    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description("add_track() deduplicates reference rows"))
BOOST_AUTO_TEST_CASE(add_track__same_artist__deduplicates)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    onelib::track_info t1;
    t1.title = "Track 1";
    t1.artist = "Same Artist";
    t1.relative_path = "/Contents/Track1.flac";
    lib.add_track(t1);

    onelib::track_info t2;
    t2.title = "Track 2";
    t2.artist = "Same Artist";
    t2.relative_path = "/Contents/Track2.flac";
    lib.add_track(t2);

    sqlite3* db = open_for_verify(tmp_loc.temp_dir);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM content") == 2);
    BOOST_TEST(query_int(db, "SELECT count(*) FROM artist") == 1);
    close_for_verify(db);
}

BOOST_TEST_DECORATOR(
    *utf::description("get_track() returns data matching what was written"))
BOOST_AUTO_TEST_CASE(get_track__round_trip__matches)
{
    temporary_directory tmp_loc;

    // Create and write a track.
    onelib::track_info written;
    written.title = "Round Trip";
    written.artist = "Test Artist";
    written.album = "Test Album";
    written.genre = "Test Genre";
    written.label = "Test Label";
    written.key = "Am";
    written.composer = "Composer Name";
    written.lyricist = "Lyricist Name";
    written.bpm = 128.5;
    written.duration_secs = 300;
    written.track_number = 5;
    written.bitrate = 320;
    written.sample_rate = 44100;
    written.file_type = 5;
    written.isrc = "GB-ABC-12-34567";
    written.year = 2024;
    written.rating = 4;
    written.relative_path = "/Contents/Test.flac";

    {
        auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);
        auto id = lib.add_track(written);
        BOOST_TEST(id == 1);
    }

    // Load and read back.
    auto lib = onelib::onelibrary::load(tmp_loc.temp_dir);
    BOOST_TEST(lib.track_count() == 1);

    auto ids = lib.track_ids();
    BOOST_TEST(ids.size() == 1);
    BOOST_TEST(ids[0] == 1);

    auto read = lib.get_track(1);
    BOOST_REQUIRE(read.has_value());

    BOOST_TEST(read->title == written.title);
    BOOST_TEST(read->artist == written.artist);
    BOOST_REQUIRE(read->album.has_value());
    BOOST_TEST(*read->album == *written.album);
    BOOST_REQUIRE(read->genre.has_value());
    BOOST_TEST(*read->genre == *written.genre);
    BOOST_REQUIRE(read->label.has_value());
    BOOST_TEST(*read->label == *written.label);
    BOOST_REQUIRE(read->key.has_value());
    BOOST_TEST(*read->key == *written.key);
    BOOST_REQUIRE(read->composer.has_value());
    BOOST_TEST(*read->composer == *written.composer);
    BOOST_REQUIRE(read->lyricist.has_value());
    BOOST_TEST(*read->lyricist == *written.lyricist);
    BOOST_TEST(read->bpm == written.bpm);
    BOOST_TEST(read->duration_secs == written.duration_secs);
    BOOST_TEST(read->track_number == written.track_number);
    BOOST_TEST(read->bitrate == written.bitrate);
    BOOST_TEST(read->sample_rate == written.sample_rate);
    BOOST_TEST(read->file_type == written.file_type);
    BOOST_REQUIRE(read->isrc.has_value());
    BOOST_TEST(*read->isrc == *written.isrc);
    BOOST_REQUIRE(read->year.has_value());
    BOOST_TEST(*read->year == *written.year);
    BOOST_TEST(read->rating == written.rating);
    BOOST_TEST(read->relative_path == written.relative_path);
}

BOOST_TEST_DECORATOR(
    *utf::description("get_track() with nonexistent id returns nullopt"))
BOOST_AUTO_TEST_CASE(get_track__nonexistent__nullopt)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);
    BOOST_TEST(!lib.get_track(999).has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("remove_track() removes a track by id"))
BOOST_AUTO_TEST_CASE(remove_track__existing__removes)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    onelib::track_info track;
    track.title = "Doomed";
    track.artist = "Test Artist";
    track.relative_path = "/Contents/Doomed.flac";
    auto id = lib.add_track(track);
    lib.remove_track(id);

    BOOST_TEST(lib.track_count() == 0);
    BOOST_TEST(!lib.get_track(id).has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("get_track_by_relative_path() round-trips added tracks"))
BOOST_AUTO_TEST_CASE(get_track_by_relative_path__existing__returns_track)
{
    temporary_directory tmp_loc;
    auto lib = onelib::onelibrary::create(tmp_loc.temp_dir);

    onelib::track_info track;
    track.title = "By Path";
    track.artist = "Test Artist";
    track.relative_path = "/Contents/ByPath.flac";
    lib.add_track(track);

    auto read = lib.get_track_by_relative_path("/Contents/ByPath.flac");
    BOOST_REQUIRE(read.has_value());
    BOOST_TEST(read->title == track.title);
    BOOST_TEST(
        !lib.get_track_by_relative_path("/Contents/Missing.flac")
             .has_value());
}
