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

#include <memory>

#include "../impl/database_impl.hpp"

namespace djinterop::onelibrary
{
struct onelibrary_context;

/// Database implementation bridging the high-level database API to OneLibrary.
class database_impl : public djinterop::database_impl
{
public:
    explicit database_impl(std::shared_ptr<onelibrary_context> context);

    std::optional<crate> crate_by_id(int64_t id) override;
    crate create_root_crate(const std::string& name) override;
    crate create_root_crate_after(
        const std::string& name, const crate& after) override;
    playlist create_root_playlist(const std::string& name) override;
    playlist create_root_playlist_after(
        const std::string& name,
        const djinterop::playlist_impl& after) override;
    track create_track(const track_snapshot& snapshot) override;
    std::string directory() override;
    void verify() override;
    void remove_crate(crate cr) override;
    void remove_playlist(const djinterop::playlist_impl& pl) override;
    void remove_track(track tr) override;
    std::vector<crate> root_crates() override;
    std::optional<crate> root_crate_by_name(
        const std::string& name) override;
    std::vector<playlist> root_playlists() override;
    std::optional<playlist> root_playlist_by_name(
        const std::string& name) override;
    std::optional<track> track_by_id(int64_t id) override;
    std::vector<track> tracks() override;
    std::vector<track> tracks_by_relative_path(
        const std::string& relative_path) override;
    std::string uuid() override;
    std::string version_name() override;

    std::shared_ptr<onelibrary_context> context() const { return context_; }

private:
    std::shared_ptr<onelibrary_context> context_;
};

}  // namespace djinterop::onelibrary