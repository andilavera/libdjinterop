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

#include "key_utils.hpp"

#include <vector>

namespace djinterop::onelibrary
{

namespace
{
// Key name → musical_key mapping (rekordbox naming convention).
// The musical_key enum follows the Camelot wheel order.
const std::vector<std::pair<const char*, musical_key>> kKeyNameMap = {
    {"C",    musical_key::c_major},
    {"Am",   musical_key::a_minor},
    {"G",    musical_key::g_major},
    {"Em",   musical_key::e_minor},
    {"D",    musical_key::d_major},
    {"Bm",   musical_key::b_minor},
    {"A",    musical_key::a_major},
    {"F#m",  musical_key::f_sharp_minor},
    {"E",    musical_key::e_major},
    {"Dbm",  musical_key::d_flat_minor},
    {"B",    musical_key::b_major},
    {"Abm",  musical_key::a_flat_minor},
    {"F#",   musical_key::f_sharp_major},
    {"Ebm",  musical_key::e_flat_minor},
    {"Db",   musical_key::d_flat_major},
    {"Bbm",  musical_key::b_flat_minor},
    {"Ab",   musical_key::a_flat_major},
    {"Fm",   musical_key::f_minor},
    {"Eb",   musical_key::e_flat_major},
    {"Cm",   musical_key::c_minor},
    {"Bb",   musical_key::b_flat_major},
    {"Gm",   musical_key::g_minor},
    {"F",    musical_key::f_major},
    {"Dm",   musical_key::d_minor},
};
}  // anonymous namespace

std::optional<musical_key> key_name_to_enum(const std::string& name)
{
    for (const auto& [n, k] : kKeyNameMap)
        if (name == n) return k;
    return std::nullopt;
}

const char* musical_key_to_name(musical_key k)
{
    for (const auto& [n, mk] : kKeyNameMap)
        if (mk == k) return n;
    return nullptr;
}

}  // namespace djinterop::onelibrary