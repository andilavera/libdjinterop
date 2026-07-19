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

#include "convert.hpp"

namespace djinterop::onelibrary::anlz::convert
{

std::vector<pqtz_entry> to_pqtz_entries(
    const std::vector<beatgrid_marker>& beatgrid, double sample_rate)
{
    if (sample_rate <= 0)
        sample_rate = 44100.0;

    const auto entry_count = static_cast<uint32_t>(beatgrid.size());
    std::vector<pqtz_entry> entries;

    if (beatgrid.empty())
    {
        // Single placeholder beat at t=0, 120 BPM.
        entries.push_back({1, 12000, 0});
        return entries;
    }

    entries.reserve(entry_count);
    for (uint32_t i = 0; i < entry_count; ++i)
    {
        uint16_t beat_num =
            static_cast<uint16_t>((beatgrid[i].index % 4) + 1);

        // Compute tempo from this marker to the next (or previous for last).
        double tempo_x100 = 12000.0;  // default 120 BPM
        if (i + 1 < entry_count)
        {
            double samples = beatgrid[i + 1].sample_offset -
                             beatgrid[i].sample_offset;
            double beats =
                beatgrid[i + 1].index - beatgrid[i].index;
            if (samples > 0 && beats > 0)
                tempo_x100 =
                    60.0 * sample_rate / (samples / beats) * 100.0;
        }
        else if (i > 0)
        {
            double samples = beatgrid[i].sample_offset -
                             beatgrid[i - 1].sample_offset;
            double beats =
                beatgrid[i].index - beatgrid[i - 1].index;
            if (samples > 0 && beats > 0)
                tempo_x100 =
                    60.0 * sample_rate / (samples / beats) * 100.0;
        }

        uint32_t time_ms =
            sample_offset_to_ms(beatgrid[i].sample_offset, sample_rate);

        entries.push_back(
            {beat_num, static_cast<uint16_t>(tempo_x100), time_ms});
    }

    return entries;
}

std::optional<pcob_entry_data> to_pcob_hot_cue_data(
    const std::optional<hot_cue>& cue, size_t slot, double sample_rate)
{
    if (!cue) return std::nullopt;

    return pcob_entry_data{
        /* .hot_cue_idx = */ static_cast<uint32_t>(slot + 1),
        /* .is_loop = */ false,
        /* .time_ms = */ sample_offset_to_ms(
            cue->sample_offset, sample_rate),
        /* .loop_time_ms = */ 0xFFFFFFFF,
    };
}

std::optional<pcob_entry_data> to_pcob_loop_data(
    const std::optional<loop>& lp, double sample_rate)
{
    if (!lp) return std::nullopt;

    uint32_t time_ms =
        sample_offset_to_ms(lp->start_sample_offset, sample_rate);
    uint32_t loop_time_ms = 0xFFFFFFFF;

    if (lp->end_sample_offset > lp->start_sample_offset)
    {
        loop_time_ms = static_cast<uint32_t>(
            (lp->end_sample_offset - lp->start_sample_offset) * 1000.0 /
            sample_rate);
    }

    return pcob_entry_data{
        /* .hot_cue_idx = */ 0,
        /* .is_loop = */ lp->end_sample_offset > lp->start_sample_offset,
        /* .time_ms = */ time_ms,
        /* .loop_time_ms = */ loop_time_ms,
    };
}

}  // namespace djinterop::onelibrary::anlz::convert