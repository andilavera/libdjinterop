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
#include "onelibrary_context.hpp"
#include "schema.hpp"

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
    key_{std::make_unique<key_table>(context_)}
{
}

onelibrary::~onelibrary() = default;

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
