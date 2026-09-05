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
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace djinterop::onelibrary
{
struct onelibrary_context;

/// Represents a row in the `content` table (SPEC §2.6.1).
struct content_row
{
    int64_t id = 0;                    ///< content_id (0 = not yet persisted)
    std::string title;                 ///< Track title
    std::optional<std::string> subtitle;
    int64_t bpmx100 = 0;               ///< BPM × 100
    int64_t length = 0;                ///< Duration in seconds
    int64_t track_no = 0;
    int64_t disc_no = 0;

    // FK references
    int64_t artist_id = 0;
    int64_t remixer_id = 0;
    int64_t original_artist_id = 0;
    int64_t composer_id = 0;
    int64_t lyricist_id = 0;
    int64_t album_id = 0;
    int64_t genre_id = 0;
    int64_t label_id = 0;
    int64_t key_id = 0;
    int64_t color_id = 0;
    int64_t image_id = 0;

    std::optional<std::string> dj_comment;
    int64_t rating = 0;
    std::optional<int64_t> release_year;
    std::optional<std::string> release_date;
    std::optional<std::string> date_created;
    std::optional<std::string> date_added;
    std::string path;                  ///< Volume-relative audio path
    std::string file_name;
    int64_t file_size = 0;
    int64_t file_type = 0;             ///< 5 = FLAC, see SPEC §2.7.1
    int64_t bitrate = 0;
    int64_t bit_depth = 0;
    int64_t sampling_rate = 0;
    std::optional<std::string> isrc;
    std::string analysis_data_file_path;  ///< Path to ANLZ .DAT file

    // Fixed magic constants (SPEC §2.4)
    int64_t analysed_bits = 41;
    int64_t content_link = 788224;

    /// Construct from the flat column list returned by a full SELECT.
    /// Parameter order must match k_select_content columns.
    static content_row from_columns(
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
        int64_t analysed_bits, int64_t content_link);
};

/// Exception thrown when a content row operation fails due to an invalid id.
struct content_row_id_error : public std::runtime_error
{
    explicit content_row_id_error(const std::string& what_arg) noexcept :
        runtime_error{what_arg} {}
};

/// Thin wrapper over the `content` table.
class content_table
{
public:
    explicit content_table(std::shared_ptr<onelibrary_context> context);

    /// Insert a content row and return its generated id.
    int64_t add(const content_row& row);

    /// Get a content row by id.
    std::optional<content_row> get(int64_t id) const;

    /// Get all content ids.
    std::vector<int64_t> all_ids() const;

private:
    std::shared_ptr<onelibrary_context> context_;
};

}  // namespace djinterop::onelibrary