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

#include "content_table.hpp"

#include <cassert>

#include <sqlite_modern_cpp.h>

#include "onelibrary_context.hpp"

namespace djinterop::onelibrary
{

namespace
{
constexpr const char* k_select_content =
    "SELECT content_id, title, subtitle, bpmx100, length, "
    "trackNo, discNo, "
    "artist_id_artist, artist_id_remixer, artist_id_originalArtist, "
    "artist_id_composer, artist_id_lyricist, "
    "album_id, genre_id, label_id, key_id, color_id, image_id, "
    "djComment, rating, releaseYear, releaseDate, "
    "dateCreated, dateAdded, path, fileName, fileSize, "
    "fileType, bitrate, bitDepth, samplingRate, isrc, "
    "analysisDataFilePath, analysedBits, contentLink "
    "FROM content WHERE content_id = ?";
}  // anonymous namespace

content_table::content_table(std::shared_ptr<onelibrary_context> context) :
    context_{std::move(context)}
{
}

int64_t content_table::add(const content_row& row)
{
    int64_t next_id = 1;
    context_->db
            << "SELECT COALESCE(MAX(content_id), 0) + 1 FROM content;" >>
        next_id;

    context_->db
        << "INSERT INTO content ("
           "content_id, title, subtitle, bpmx100, length, trackNo, discNo, "
           "artist_id_artist, artist_id_remixer, artist_id_originalArtist, "
           "artist_id_composer, artist_id_lyricist, "
           "album_id, genre_id, label_id, key_id, color_id, image_id, "
           "djComment, rating, releaseYear, releaseDate, dateCreated, "
           "dateAdded, path, fileName, fileSize, fileType, bitrate, "
           "bitDepth, samplingRate, isrc, "
           "djPlayCount, isHotCueAutoLoadOn, isKuvoDeliverStatusOn, "
           "kuvoDeliveryComment, masterDbId, masterContentId, "
           "analysisDataFilePath, analysedBits, contentLink, hasModified"
           ") VALUES ("
           "?, ?, ?, ?, ?, ?, ?, "
           "?, ?, ?, ?, ?, "
           "?, ?, ?, ?, ?, ?, "
           "?, ?, ?, ?, ?, "
           "?, ?, ?, ?, ?, ?, "
           "?, ?, ?, "
           "0, 0, 0, "
           "'', 0, 0, "
           "?, ?, ?, 0"
           ");"
        << next_id << row.title << row.subtitle << row.bpmx100 << row.length
        << row.track_no << row.disc_no << row.artist_id << row.remixer_id
        << row.original_artist_id << row.composer_id << row.lyricist_id
        << row.album_id << row.genre_id << row.label_id << row.key_id
        << row.color_id << row.image_id << row.dj_comment << row.rating
        << row.release_year << row.release_date << row.date_created
        << row.date_added << row.path << row.file_name << row.file_size
        << row.file_type << row.bitrate << row.bit_depth << row.sampling_rate
        << row.isrc << row.analysis_data_file_path
        << row.analysed_bits << row.content_link;

    return next_id;
}

std::optional<content_row> content_table::get(int64_t id) const
{
    std::optional<content_row> result;

    context_->db << k_select_content << id >> [&](
        int64_t content_id, std::string title,
        std::optional<std::string> subtitle, int64_t bpmx100,
        int64_t length, int64_t track_no, int64_t disc_no,
        int64_t artist_id, int64_t remixer_id,
        int64_t original_artist_id, int64_t composer_id,
        int64_t lyricist_id, int64_t album_id, int64_t genre_id,
        int64_t label_id, int64_t key_id, int64_t color_id,
        int64_t image_id, std::optional<std::string> dj_comment,
        int64_t rating, std::optional<int64_t> release_year,
        std::optional<std::string> release_date,
        std::optional<std::string> date_created,
        std::optional<std::string> date_added, std::string path,
        std::string file_name, int64_t file_size, int64_t file_type,
        int64_t bitrate, int64_t bit_depth, int64_t sampling_rate,
        std::optional<std::string> isrc,
        std::string analysis_data_file_path,
        int64_t analysed_bits, int64_t content_link)
    {
        assert(!result);
        result = content_row::from_columns(
            content_id, std::move(title), std::move(subtitle),
            bpmx100, length, track_no, disc_no,
            artist_id, remixer_id, original_artist_id,
            composer_id, lyricist_id, album_id, genre_id,
            label_id, key_id, color_id, image_id,
            std::move(dj_comment), rating, release_year,
            std::move(release_date), std::move(date_created),
            std::move(date_added), std::move(path),
            std::move(file_name), file_size, file_type,
            bitrate, bit_depth, sampling_rate, std::move(isrc),
            std::move(analysis_data_file_path),
            analysed_bits, content_link);
    };

    return result;
}

std::vector<int64_t> content_table::all_ids() const
{
    std::vector<int64_t> ids;
    context_->db << "SELECT content_id FROM content ORDER BY content_id;" >>
        [&](int64_t id) { ids.push_back(id); };
    return ids;
}

}  // namespace djinterop::onelibrary

// =============================================================================
// content_row::from_columns
// =============================================================================
namespace djinterop::onelibrary
{

content_row content_row::from_columns(
    int64_t content_id, std::string title,
    std::optional<std::string> subtitle, int64_t bpmx100,
    int64_t length, int64_t track_no, int64_t disc_no,
    int64_t artist_id, int64_t remixer_id,
    int64_t original_artist_id, int64_t composer_id,
    int64_t lyricist_id, int64_t album_id, int64_t genre_id,
    int64_t label_id, int64_t key_id, int64_t color_id,
    int64_t image_id, std::optional<std::string> dj_comment,
    int64_t rating, std::optional<int64_t> release_year,
    std::optional<std::string> release_date,
    std::optional<std::string> date_created,
    std::optional<std::string> date_added, std::string path,
    std::string file_name, int64_t file_size, int64_t file_type,
    int64_t bitrate, int64_t bit_depth, int64_t sampling_rate,
    std::optional<std::string> isrc,
    std::string analysis_data_file_path,
    int64_t analysed_bits, int64_t content_link)
{
    return content_row{
        content_id,
        std::move(title),
        std::move(subtitle),
        bpmx100,
        length,
        track_no,
        disc_no,
        artist_id,
        remixer_id,
        original_artist_id,
        composer_id,
        lyricist_id,
        album_id,
        genre_id,
        label_id,
        key_id,
        color_id,
        image_id,
        std::move(dj_comment),
        rating,
        release_year,
        std::move(release_date),
        std::move(date_created),
        std::move(date_added),
        std::move(path),
        std::move(file_name),
        file_size,
        file_type,
        bitrate,
        bit_depth,
        sampling_rate,
        std::move(isrc),
        std::move(analysis_data_file_path),
        analysed_bits,
        content_link,
    };
}

}  // namespace djinterop::onelibrary