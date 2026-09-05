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
