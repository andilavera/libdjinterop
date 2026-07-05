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
#include <string>

namespace djinterop::onelibrary::anlz
{

/// Result of hashing a volume-relative audio path to determine its
/// USBANLZ directory (SPEC §2.11.1).
struct anlz_path
{
    /// The `P` directory component (0–127).
    uint32_t p;

    /// The 8-hex-digit subdirectory component.
    uint32_t part2;

    /// Formatted directory relative to `.PIONEER/USBANLZ/`,
    /// e.g. `P039/000272B9`.
    std::string to_directory() const;
};

/// Compute the USBANLZ directory hash from a volume-relative audio path.
///
/// The path must use forward slashes, begin with `/`, and be UTF-8.
/// The hash iterates over Unicode code points (decoded from UTF-8),
/// matching the algorithm verified against CDJ-3000 firmware 3.19.
anlz_path compute_anlz_path(const std::string& file_path);

}  // namespace djinterop::onelibrary::anlz