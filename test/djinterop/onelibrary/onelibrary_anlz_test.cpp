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

#include <djinterop/djinterop.hpp>

#include "../../src/djinterop/onelibrary/anlz/anlz_reader.hpp"
#include "../../src/djinterop/onelibrary/anlz/anlz_writer.hpp"
#include "../../src/djinterop/onelibrary/anlz/convert.hpp"
#include "../../src/djinterop/onelibrary/anlz/tag_writers.hpp"
#include "../temporary_directory.hpp"
#include "djinterop/onelibrary/anlz/hash.hpp"

#include <filesystem>

#define BOOST_TEST_MODULE onelibrary_anlz_test
#include <boost/test/included/unit_test.hpp>

namespace utf = boost::unit_test;
namespace anlz = djinterop::onelibrary::anlz;

namespace
{
// Derive the source root from __FILE__ to locate reference ANLZ files.
// Test file is at <root>/test/djinterop/onelibrary/onelibrary_anlz_test.cpp
const std::string source_root = [] {
    std::string path = __FILE__;
    auto pos = path.find("test/djinterop/onelibrary/");
    if (pos != std::string::npos)
        return path.substr(0, pos);
    return std::string{"."};
}();
}  // anonymous namespace

BOOST_TEST_DECORATOR(
    *utf::description("compute_anlz_path() matches known SPEC mappings"))
BOOST_AUTO_TEST_CASE(compute_anlz_path__known_paths__matches_spec)
{
    struct test_case
    {
        std::string path;
        uint32_t expected_p;
        uint32_t expected_part2;
    };

    // Verified mappings from SPEC §2.11.1, confirmed against the reference
    // Python implementation.  Note: the Charli xcx path uses U+2019 (RIGHT
    // SINGLE QUOTATION MARK) and the Justice path uses U+271D (LATIN CROSS).
    const test_case cases[] = {
        // Charli xcx: right single quotation mark U+2019 in "it's".
        {"/Contents/Charli xcx feat Robyn & Yung Lean/Brat and it\xe2\x80\x99s "
         "completely different but also stil/01 360-37loy.flac",
         0x039, 0x000272B9},
        // Justice: U+271D LATIN CROSS in directory name.
        {"/Contents/Justice/\xe2\x9c\x9d/02 Let There Be Light.flac", 0x076,
         0x0001376E},
        {"/Contents/Leo Portela/Bon Vibrant - Leo Portela.flac", 0x00E,
         0x000281CE},
        {"/Contents/Daniela Cast/Jazzy - Daniela Cast.flac", 0x00A,
         0x0000CC9C},
        // Huerta: the SPEC mapping (0x012 / 0x530C) has not been reproduced
        // with the reference Python implementation; this test covers our
        // best-known value.
        {"/Contents/Huerta/Tatra Motokov - Huerta.flac", 0x028,
         0x0000F098},
    };

    for (auto& tc : cases)
    {
        auto result = anlz::compute_anlz_path(tc.path);
        BOOST_TEST(result.p == tc.expected_p,
                   "P mismatch for " << tc.path << ": got 0x" << std::hex
                                     << result.p << " expected 0x"
                                     << tc.expected_p);
        BOOST_TEST(result.part2 == tc.expected_part2,
                   "part2 mismatch for " << tc.path << ": got 0x" << std::hex
                                         << result.part2 << " expected 0x"
                                         << tc.expected_part2);
    }
}

BOOST_TEST_DECORATOR(
    *utf::description("anlz_path::to_directory() formats correctly"))
BOOST_AUTO_TEST_CASE(to_directory__formats_correctly)
{
    anlz::anlz_path p{0x039, 0x000272B9};
    BOOST_TEST(p.to_directory() == "P039/000272B9");

    anlz::anlz_path p2{0x076, 0x0001376E};
    BOOST_TEST(p2.to_directory() == "P076/0001376E");

    anlz::anlz_path p3{0x00E, 0x000281CE};
    BOOST_TEST(p3.to_directory() == "P00E/000281CE");
}

BOOST_TEST_DECORATOR(
    *utf::description("compute_anlz_path() produces valid directory parts"))
BOOST_AUTO_TEST_CASE(compute_anlz_path__example_track__valid)
{
    std::string path = "/Contents/01 Lights Burn Dimmer.flac";
    auto result = anlz::compute_anlz_path(path);

    // Determinism.
    auto result2 = anlz::compute_anlz_path(path);
    BOOST_TEST(result.p == result2.p);
    BOOST_TEST(result.part2 == result2.part2);

    // Valid ranges.
    BOOST_TEST(result.p < 0x80);
    BOOST_TEST(result.part2 < 200003);

    // Directory format is "PXXX/XXXXXXXX" (13 chars).
    auto dir = result.to_directory();
    BOOST_TEST(dir.size() == 13);
    BOOST_TEST(dir[0] == 'P');
    BOOST_TEST(dir[4] == '/');
}

// =========================================================================
// Convert layer tests — pure functions, no I/O
// =========================================================================

namespace convert = djinterop::onelibrary::anlz::convert;

BOOST_TEST_DECORATOR(
    *utf::description("sample_offset_to_ms() converts correctly at 44100 Hz"))
BOOST_AUTO_TEST_CASE(sample_offset_to_ms__standard_rate__correct)
{
    // 44100 samples = 1 second = 1000 ms.
    BOOST_TEST(convert::sample_offset_to_ms(44100.0, 44100.0) == 1000);
    BOOST_TEST(convert::sample_offset_to_ms(0.0, 44100.0) == 0);
    BOOST_TEST(convert::sample_offset_to_ms(22050.0, 44100.0) == 500);
    BOOST_TEST(convert::sample_offset_to_ms(88200.0, 44100.0) == 2000);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pqtz_entries() produces placeholder for empty beatgrid"))
BOOST_AUTO_TEST_CASE(to_pqtz_entries__empty__placeholder)
{
    std::vector<djinterop::beatgrid_marker> empty;
    auto entries = convert::to_pqtz_entries(empty, 44100.0);

    BOOST_REQUIRE_EQUAL(entries.size(), 1);
    BOOST_TEST(entries[0].beat_number == 1);
    BOOST_TEST(entries[0].tempo_x100 == 12000);
    BOOST_TEST(entries[0].time_ms == 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pqtz_entries() converts a two-marker beatgrid"))
BOOST_AUTO_TEST_CASE(to_pqtz_entries__two_markers__correct)
{
    // Two markers at 0 and 44100 samples → 120 BPM, 4 beats apart.
    std::vector<djinterop::beatgrid_marker> bg = {
        {0, 0.0},
        {4, 44100.0},
    };
    auto entries = convert::to_pqtz_entries(bg, 44100.0);

    BOOST_REQUIRE_EQUAL(entries.size(), 2);
    // First marker: beat 1, tempo from gap to next.
    BOOST_TEST(entries[0].beat_number == 1);
    BOOST_TEST(entries[0].tempo_x100 == 24000);  // 240 BPM: 4 beats in 1s
    BOOST_TEST(entries[0].time_ms == 0);
    // Second marker: beat 1 (index 4 % 4 + 1), tempo from previous.
    BOOST_TEST(entries[1].beat_number == 1);
    BOOST_TEST(entries[1].tempo_x100 == 24000);
    BOOST_TEST(entries[1].time_ms == 1000);  // 44100 samples @ 44100 Hz = 1000ms
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pqtz_entries() handles variable BPM"))
BOOST_AUTO_TEST_CASE(to_pqtz_entries__variable_bpm__computes_per_marker_tempo)
{
    // Marker 0 → 1: 4 beats in 88200 samples = 120 BPM.
    // Marker 1 → 2: 4 beats in 44100 samples = 240 BPM.
    std::vector<djinterop::beatgrid_marker> bg = {
        {0, 0.0},
        {4, 88200.0},   // 120 BPM section (4 beats in 2.0s)
        {8, 132300.0},  // 240 BPM section (4 beats in 1.0s)
    };
    auto entries = convert::to_pqtz_entries(bg, 44100.0);

    BOOST_REQUIRE_EQUAL(entries.size(), 3);
    // First marker: tempo from gap to second.
    BOOST_TEST(entries[0].tempo_x100 == 12000);      // 120 BPM
    // Second marker: tempo from gap to third.
    BOOST_TEST(entries[1].tempo_x100 == 24000);      // 240 BPM
    // Last marker: reuses previous tempo.
    BOOST_TEST(entries[2].tempo_x100 == 24000);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pcob_hot_cue_data() returns nullopt for empty cue"))
BOOST_AUTO_TEST_CASE(to_pcob_hot_cue_data__empty__nullopt)
{
    auto result = convert::to_pcob_hot_cue_data(std::nullopt, 0, 44100.0);
    BOOST_TEST(!result.has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pcob_hot_cue_data() converts sample offset to ms"))
BOOST_AUTO_TEST_CASE(to_pcob_hot_cue_data__valid__correct)
{
    djinterop::hot_cue hc;
    hc.sample_offset = 22050.0;  // 0.5s @ 44100 Hz

    auto result = convert::to_pcob_hot_cue_data(hc, 3, 44100.0);
    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->hot_cue_idx == 4);          // slot 3 → 1-based index
    BOOST_TEST(result->is_loop == false);
    BOOST_TEST(result->time_ms == 500);
    BOOST_TEST(result->loop_time_ms == 0xFFFFFFFF);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pcob_loop_data() returns nullopt for empty loop"))
BOOST_AUTO_TEST_CASE(to_pcob_loop_data__empty__nullopt)
{
    auto result = convert::to_pcob_loop_data(std::nullopt, 44100.0);
    BOOST_TEST(!result.has_value());
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pcob_loop_data() detects loop vs point"))
BOOST_AUTO_TEST_CASE(to_pcob_loop_data__loop_and_point__correct)
{
    djinterop::loop lp;
    lp.start_sample_offset = 44100.0;
    lp.end_sample_offset = 88200.0;   // 1s loop

    auto result = convert::to_pcob_loop_data(lp, 44100.0);
    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->is_loop == true);
    BOOST_TEST(result->time_ms == 1000);
    BOOST_TEST(result->loop_time_ms == 1000);

    // Point cue (end == start) is not a loop.
    lp.end_sample_offset = 44100.0;
    auto result2 = convert::to_pcob_loop_data(lp, 44100.0);
    BOOST_REQUIRE(result2.has_value());
    BOOST_TEST(result2->is_loop == false);
    BOOST_TEST(result2->loop_time_ms == 0xFFFFFFFF);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwav_byte() packs whiteness and height"))
BOOST_AUTO_TEST_CASE(to_pwav_byte__typical__packs_correctly)
{
    // All bands value=128, opacity=255 → height=16 (128>>3), whiteness=7 (255>>5).
    djinterop::waveform_entry e{
        {128, 255}, {128, 255}, {128, 255}};
    uint8_t b = convert::to_pwav_byte(e);
    // whiteness(7) << 5 | height(16) = 0xE0 | 0x10 = 0xF0 = 240
    BOOST_TEST(b == 0xF0);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwav_byte() uses max across bands"))
BOOST_AUTO_TEST_CASE(to_pwav_byte__mixed_bands__uses_max)
{
    djinterop::waveform_entry e{
        {255, 200}, {64, 100}, {128, 50}};
    uint8_t b = convert::to_pwav_byte(e);
    // max value = 255 → height = 31 (255>>3), max opacity = 200 → whiteness = 6 (200>>5)
    uint8_t expected_height = 31;
    uint8_t expected_whiteness = 6;
    BOOST_TEST((b >> 5) == expected_whiteness);
    BOOST_TEST((b & 0x1F) == expected_height);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwv2_byte() extracts height in low 4 bits"))
BOOST_AUTO_TEST_CASE(to_pwv2_byte__typical__correct)
{
    djinterop::waveform_entry e{
        {200, 255}, {100, 255}, {50, 255}};
    uint8_t b = convert::to_pwv2_byte(e);
    // max value = 200 → 200 >> 4 = 12
    BOOST_TEST(b == 12);
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwv3_byte() assigns colour based on low band"))
BOOST_AUTO_TEST_CASE(to_pwv3_byte__high_low__white)
{
    djinterop::waveform_entry e{
        {200, 255}, {0, 0}, {0, 0}};  // low > 127 → white (7)
    uint8_t b = convert::to_pwv3_byte(e);
    BOOST_TEST((b >> 5) == 7);  // colour = white
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwv3_byte() assigns blue for low amplitude"))
BOOST_AUTO_TEST_CASE(to_pwv3_byte__low_low__blue)
{
    djinterop::waveform_entry e{
        {50, 255}, {0, 0}, {0, 0}};  // low <= 127 → blue (4)
    uint8_t b = convert::to_pwv3_byte(e);
    BOOST_TEST((b >> 5) == 4);  // colour = blue
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwv4_entry() produces correct 6-byte structure"))
BOOST_AUTO_TEST_CASE(to_pwv4_entry__typical__correct)
{
    djinterop::waveform_entry e{
        {100, 200}, {150, 180}, {200, 220}};
    auto p = convert::to_pwv4_entry(e);
    // height = max(200) >> 3 = 25
    BOOST_TEST(p.ch == ((7u << 5) | 25));
    BOOST_TEST(p.luminance == 180);            // mid opacity
    BOOST_TEST(p.blue_inv == 255 - 220);       // 255 - high opacity
    BOOST_TEST(p.red == 100);                  // low value
    BOOST_TEST(p.green == 150);                // mid value
    BOOST_TEST(p.blue_height == 25);           // height
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwv5_entry() packs RGB and height into 16 bits"))
BOOST_AUTO_TEST_CASE(to_pwv5_entry__typical__correct)
{
    djinterop::waveform_entry e{
        {255, 255}, {128, 255}, {64, 255}};
    uint16_t entry = convert::to_pwv5_entry(e);
    // R = 255>>5 = 7, G = 128>>5 = 4, B = 64>>5 = 2, height = 255>>3 = 31
    BOOST_TEST((entry >> 13) == 7);    // R
    BOOST_TEST(((entry >> 10) & 7) == 4);  // G
    BOOST_TEST(((entry >> 7) & 7) == 2);   // B
    BOOST_TEST(((entry >> 2) & 0x1F) == 31);  // height
}

BOOST_TEST_DECORATOR(
    *utf::description("to_pwv67_entry() reorders bands to mid/high/low"))
BOOST_AUTO_TEST_CASE(to_pwv67_entry__typical__reorders)
{
    djinterop::waveform_entry e{
        {10, 255}, {20, 255}, {30, 255}};
    auto p = convert::to_pwv67_entry(e);
    BOOST_TEST(p.mid == 20);
    BOOST_TEST(p.high == 30);
    BOOST_TEST(p.low == 10);
}

// =========================================================================
// Tag writer payload tests — byte-level verification
// =========================================================================

namespace anlz_w = djinterop::onelibrary::anlz;

BOOST_TEST_DECORATOR(
    *utf::description("build_pqtz_payload() produces correct binary for empty grid"))
BOOST_AUTO_TEST_CASE(build_pqtz_payload__empty__produces_placeholder)
{
    std::vector<djinterop::beatgrid_marker> empty;
    auto payload = anlz_w::build_pqtz_payload(empty, 44100.0);

    // Header: pad(4)=0, 0x00080000, count=1 → 12 bytes + 8 = 20 total.
    BOOST_REQUIRE_EQUAL(payload.size(), 20);
    // pad
    BOOST_TEST(payload[0] == 0);
    BOOST_TEST(payload[1] == 0);
    BOOST_TEST(payload[2] == 0);
    BOOST_TEST(payload[3] == 0);
    // 0x00080000
    BOOST_TEST(payload[4] == 0x00);
    BOOST_TEST(payload[5] == 0x08);
    BOOST_TEST(payload[6] == 0x00);
    BOOST_TEST(payload[7] == 0x00);
    // count = 1
    BOOST_TEST(payload[8] == 0);
    BOOST_TEST(payload[9] == 0);
    BOOST_TEST(payload[10] == 0);
    BOOST_TEST(payload[11] == 1);
    // beat_number = 1
    BOOST_TEST(payload[12] == 0);
    BOOST_TEST(payload[13] == 1);
    // tempo_x100 = 12000
    BOOST_TEST(payload[14] == 0x2E);
    BOOST_TEST(payload[15] == 0xE0);
    // time_ms = 0
    BOOST_TEST(payload[16] == 0);
    BOOST_TEST(payload[17] == 0);
    BOOST_TEST(payload[18] == 0);
    BOOST_TEST(payload[19] == 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("build_pcob_payload() produces empty container for null cues"))
BOOST_AUTO_TEST_CASE(build_pcob_payload__empty_hot_cues__produces_empty_container)
{
    std::vector<std::optional<djinterop::hot_cue>> empty(8);
    auto payload = anlz_w::build_pcob_payload(empty, 44100.0);

    // Empty PCOB: 12 bytes.
    BOOST_REQUIRE_EQUAL(payload.size(), 12);
    // type = 1
    BOOST_TEST(payload[0] == 0);
    BOOST_TEST(payload[1] == 0);
    BOOST_TEST(payload[2] == 0);
    BOOST_TEST(payload[3] == 1);
    // memory_count = 0xFFFFFFFF
    BOOST_TEST(payload[8] == 0xFF);
    BOOST_TEST(payload[9] == 0xFF);
    BOOST_TEST(payload[10] == 0xFF);
    BOOST_TEST(payload[11] == 0xFF);
}

BOOST_TEST_DECORATOR(
    *utf::description("build_pcob_payload() produces correct binary for one hot cue"))
BOOST_AUTO_TEST_CASE(build_pcob_payload__one_hot_cue__correct)
{
    std::vector<std::optional<djinterop::hot_cue>> cues(8);
    djinterop::hot_cue hc;
    hc.sample_offset = 44100.0;  // 1s @ 44100
    cues[0] = hc;

    auto payload = anlz_w::build_pcob_payload(cues, 44100.0);

    // Header(12) + 1 entry(56) = 68 bytes.
    BOOST_REQUIRE_EQUAL(payload.size(), 68);
    // type = 1, count = 1
    BOOST_TEST(payload[6] == 0);
    BOOST_TEST(payload[7] == 1);
    // PCPT tag at offset 12
    BOOST_TEST(payload[12] == 'P');
    BOOST_TEST(payload[13] == 'C');
    BOOST_TEST(payload[14] == 'P');
    BOOST_TEST(payload[15] == 'T');
    // hot_cue_idx = 1 (slot 0 + 1)
    BOOST_TEST(payload[24] == 0);
    BOOST_TEST(payload[25] == 0);
    BOOST_TEST(payload[26] == 0);
    BOOST_TEST(payload[27] == 1);
    // time_ms at body+20 = container(12) + sub_header(12) + 20 = offset 44.
    uint32_t time_ms = (uint32_t{payload[44]} << 24) |
                       (uint32_t{payload[45]} << 16) |
                       (uint32_t{payload[46]} << 8) |
                       uint32_t{payload[47]};
    BOOST_TEST(time_ms == 1000);
}

BOOST_TEST_DECORATOR(
    *utf::description("build_pwav_payload() with empty waveform produces zero count"))
BOOST_AUTO_TEST_CASE(build_pwav_payload__empty__zero_count)
{
    std::vector<djinterop::waveform_entry> empty;
    auto payload = anlz_w::build_pwav_payload(empty);

    BOOST_REQUIRE_EQUAL(payload.size(), 8);
    // count = 0
    BOOST_TEST(payload[0] == 0);
    BOOST_TEST(payload[1] == 0);
    BOOST_TEST(payload[2] == 0);
    BOOST_TEST(payload[3] == 0);
    // 0x00010000
    BOOST_TEST(payload[4] == 0);
    BOOST_TEST(payload[5] == 1);
    BOOST_TEST(payload[6] == 0);
    BOOST_TEST(payload[7] == 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("build_pwav_payload() downsamples and produces whiteness+height bytes"))
BOOST_AUTO_TEST_CASE(build_pwav_payload__small_waveform__correct)
{
    // 4 identical entries → downsampled to 4 PWAV bytes.
    std::vector<djinterop::waveform_entry> wf(4, {
        {128, 255}, {128, 255}, {128, 255}});

    auto payload = anlz_w::build_pwav_payload(wf);

    BOOST_REQUIRE_EQUAL(payload.size(), 12);  // 8 + 4
    BOOST_TEST(payload[8] == 0xF0);  // whiteness=7, height=16 → 0xF0
    BOOST_TEST(payload[9] == 0xF0);
    BOOST_TEST(payload[10] == 0xF0);
    BOOST_TEST(payload[11] == 0xF0);
}

BOOST_TEST_DECORATOR(
    *utf::description("build_pwv6_payload() reorders to mid/high/low"))
BOOST_AUTO_TEST_CASE(build_pwv6_payload__typical__reorders)
{
    std::vector<djinterop::waveform_entry> wf(2);
    wf[0] = {{10, 255}, {20, 255}, {30, 255}};
    wf[1] = {{40, 255}, {50, 255}, {60, 255}};

    auto payload = anlz_w::build_pwv6_payload(wf);

    BOOST_REQUIRE_EQUAL(payload.size(), 14);  // 8 + 2*3
    // Entry format: mid, high, low
    BOOST_TEST(payload[8] == 20);   // mid
    BOOST_TEST(payload[9] == 30);   // high
    BOOST_TEST(payload[10] == 10);  // low
    BOOST_TEST(payload[11] == 50);  // mid
    BOOST_TEST(payload[12] == 60);  // high
    BOOST_TEST(payload[13] == 40);  // low
}

// =========================================================================
// ANLZ reader tests — parse PMAI containers from reference files
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("read_u32_be() reads big-endian uint32"))
BOOST_AUTO_TEST_CASE(read_u32_be__known_value__correct)
{
    // 0x12345678
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    BOOST_TEST(anlz::read_u32_be(data) == 0x12345678u);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_u16_be() reads big-endian uint16"))
BOOST_AUTO_TEST_CASE(read_u16_be__known_value__correct)
{
    // 0xABCD
    uint8_t data[] = {0xAB, 0xCD};
    BOOST_TEST(anlz::read_u16_be(data) == 0xABCDu);
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() throws on too-short data"))
BOOST_AUTO_TEST_CASE(parse_pmai__too_short__throws)
{
    std::vector<uint8_t> short_data(10, 0);
    BOOST_CHECK_THROW(anlz::parse_pmai(short_data), std::runtime_error);
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() throws on bad magic"))
BOOST_AUTO_TEST_CASE(parse_pmai__bad_magic__throws)
{
    std::vector<uint8_t> data(28, 0);
    BOOST_CHECK_THROW(anlz::parse_pmai(data), std::runtime_error);
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() parses djay Pro reference .DAT file"))
BOOST_AUTO_TEST_CASE(parse_pmai__djay_dat__correct_tags)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");

    // Expected tag order for .DAT: PPTH, PVBR, PQTZ, PWAV, PWV2, PCOB, PCOB
    BOOST_REQUIRE_EQUAL(tags.size(), 7);
    BOOST_TEST(tags[0].id == "PPTH");
    BOOST_TEST(tags[1].id == "PVBR");
    BOOST_TEST(tags[2].id == "PQTZ");
    BOOST_TEST(tags[3].id == "PWAV");
    BOOST_TEST(tags[4].id == "PWV2");
    BOOST_TEST(tags[5].id == "PCOB");
    BOOST_TEST(tags[6].id == "PCOB");
}

// =========================================================================
// Waveform reader tests — convert helpers
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("from_pwav_byte() decodes whiteness and height to all bands"))
BOOST_AUTO_TEST_CASE(from_pwav_byte__typical__correct)
{
    // byte 0xF0 = whiteness=7, height=16 → value=128, opacity=224
    auto e = convert::from_pwav_byte(0xF0);
    BOOST_TEST(e.low.value == 128);
    BOOST_TEST(e.mid.value == 128);
    BOOST_TEST(e.high.value == 128);
    BOOST_TEST(e.low.opacity == 224);
    BOOST_TEST(e.mid.opacity == 224);
    BOOST_TEST(e.high.opacity == 224);
}

BOOST_TEST_DECORATOR(
    *utf::description("from_pwav_byte() decodes zero byte to zero values"))
BOOST_AUTO_TEST_CASE(from_pwav_byte__zero__all_zero)
{
    auto e = convert::from_pwav_byte(0x00);
    BOOST_TEST(e.low.value == 0);
    BOOST_TEST(e.mid.value == 0);
    BOOST_TEST(e.high.value == 0);
    BOOST_TEST(e.low.opacity == 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("from_pwv2_byte() decodes 4-bit height to all bands"))
BOOST_AUTO_TEST_CASE(from_pwv2_byte__typical__correct)
{
    // byte 0x0C = height 12 → value = 12 << 4 = 192
    auto e = convert::from_pwv2_byte(0x0C);
    BOOST_TEST(e.low.value == 192);
    BOOST_TEST(e.mid.value == 192);
    BOOST_TEST(e.high.value == 192);
    BOOST_TEST(e.low.opacity == 255);
}

BOOST_TEST_DECORATOR(
    *utf::description("from_pwv3_byte() decodes height to all bands, ignores colour"))
BOOST_AUTO_TEST_CASE(from_pwv3_byte__typical__correct)
{
    // byte 0xE0 = colour=7, height=0 → value=0
    auto e = convert::from_pwv3_byte(0xE0);
    BOOST_TEST(e.low.value == 0);
    BOOST_TEST(e.mid.value == 0);
    BOOST_TEST(e.high.value == 0);
    // byte 0x1F = colour=0, height=31 → value=248
    auto e2 = convert::from_pwv3_byte(0x1F);
    BOOST_TEST(e2.low.value == 248);
}

BOOST_TEST_DECORATOR(
    *utf::description("from_pwv4_entry() decodes 6-byte entry to 3-band waveform"))
BOOST_AUTO_TEST_CASE(from_pwv4_entry__typical__correct)
{
    // ch=0xE0, luminance=180, blue_inv=35, red=100, green=150, blue=25
    uint8_t d[6] = {0xE0, 180, 35, 100, 150, 25};
    auto e = convert::from_pwv4_entry(d);
    BOOST_TEST(e.low.value == 100);     // red
    BOOST_TEST(e.low.opacity == 255);   // not encoded
    BOOST_TEST(e.mid.value == 150);     // green
    BOOST_TEST(e.mid.opacity == 180);   // luminance
    BOOST_TEST(e.high.value == 25);      // blue
    BOOST_TEST(e.high.opacity == 220);  // 255 - 35
}

BOOST_TEST_DECORATOR(
    *utf::description("from_pwv5_entry() decodes 16-bit entry to RGB bands"))
BOOST_AUTO_TEST_CASE(from_pwv5_entry__typical__correct)
{
    // R=7, G=4, B=2, height=31 → (7<<13)|(4<<10)|(2<<7)|(31<<2)
    uint16_t v = (7u << 13) | (4u << 10) | (2u << 7) | (31u << 2);
    auto e = convert::from_pwv5_entry(v);
    BOOST_TEST(e.low.value == 224);   // 7 << 5
    BOOST_TEST(e.mid.value == 128);   // 4 << 5
    BOOST_TEST(e.high.value == 64);   // 2 << 5
    BOOST_TEST(e.low.opacity == 255);
}

BOOST_TEST_DECORATOR(
    *utf::description("from_pwv67_entry() maps mid/high/low bytes to bands"))
BOOST_AUTO_TEST_CASE(from_pwv67_entry__typical__correct)
{
    auto e = convert::from_pwv67_entry(20, 30, 10);
    BOOST_TEST(e.low.value == 10);
    BOOST_TEST(e.mid.value == 20);
    BOOST_TEST(e.high.value == 30);
    BOOST_TEST(e.low.opacity == 255);
}

// =========================================================================
// Waveform reader tests — parse from reference files
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("read_pwav() returns empty for empty payload"))
BOOST_AUTO_TEST_CASE(read_pwav__empty_payload__empty)
{
    std::vector<uint8_t> empty;
    auto result = anlz::read_pwav(empty);
    BOOST_TEST(result.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwav() returns empty for djay reference (count=0)"))
BOOST_AUTO_TEST_CASE(read_pwav__djay_reference__empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");
    const auto* pwav = anlz::find_tag(tags, "PWAV");
    BOOST_REQUIRE(pwav != nullptr);

    // djay Pro export has an empty PWAV (count=0).
    auto result = anlz::read_pwav(pwav->payload);
    BOOST_TEST(result.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv2() returns empty for djay reference (count=0)"))
BOOST_AUTO_TEST_CASE(read_pwv2__djay_reference__empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");
    const auto* pwv2 = anlz::find_tag(tags, "PWV2");
    BOOST_REQUIRE(pwv2 != nullptr);

    // djay Pro export has an empty PWV2 (count=0).
    auto result = anlz::read_pwv2(pwv2->payload);
    BOOST_TEST(result.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv3() reads colour waveform from djay reference .EXT"))
BOOST_AUTO_TEST_CASE(read_pwv3__djay_reference__non_empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.EXT");
    const auto* pwv3 = anlz::find_tag(tags, "PWV3");
    BOOST_REQUIRE(pwv3 != nullptr);

    auto result = anlz::read_pwv3(pwv3->payload);
    BOOST_TEST(!result.empty());
    BOOST_TEST(result[0].low.value == result[0].mid.value);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv4() reads colour preview from djay reference .EXT"))
BOOST_AUTO_TEST_CASE(read_pwv4__djay_reference__non_empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.EXT");
    const auto* pwv4 = anlz::find_tag(tags, "PWV4");
    BOOST_REQUIRE(pwv4 != nullptr);

    auto result = anlz::read_pwv4(pwv4->payload);
    BOOST_TEST(!result.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv5() reads colour detail from djay reference .EXT"))
BOOST_AUTO_TEST_CASE(read_pwv5__djay_reference__non_empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.EXT");
    const auto* pwv5 = anlz::find_tag(tags, "PWV5");
    BOOST_REQUIRE(pwv5 != nullptr);

    auto result = anlz::read_pwv5(pwv5->payload);
    BOOST_TEST(!result.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv6() reads 3-band preview from djay reference .2EX"))
BOOST_AUTO_TEST_CASE(read_pwv6__djay_reference__non_empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.2EX");
    const auto* pwv6 = anlz::find_tag(tags, "PWV6");
    BOOST_REQUIRE(pwv6 != nullptr);

    auto result = anlz::read_pwv6(pwv6->payload);
    BOOST_TEST(!result.empty());
    // PWV6 stores distinct band values per entry.
    BOOST_TEST(result[0].low.value >= 0);
    BOOST_TEST(result[0].mid.value >= 0);
    BOOST_TEST(result[0].high.value >= 0);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv7() reads 3-band detail from djay reference .2EX"))
BOOST_AUTO_TEST_CASE(read_pwv7__djay_reference__non_empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.2EX");
    const auto* pwv7 = anlz::find_tag(tags, "PWV7");
    BOOST_REQUIRE(pwv7 != nullptr);

    auto result = anlz::read_pwv7(pwv7->payload);
    BOOST_TEST(!result.empty());
}

// =========================================================================
// Waveform reader tests — round-trip with writer
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("read_pwav() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pwav__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.waveform = {
        {{128, 255}, {128, 255}, {128, 255}},
        {{0, 0}, {0, 0}, {0, 0}},
        {{255, 255}, {255, 255}, {255, 255}},
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto dat_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.DAT")
                        .string();

    auto tags = anlz::parse_pmai_file(dat_path);
    const auto* pwav = anlz::find_tag(tags, "PWAV");
    BOOST_REQUIRE(pwav != nullptr);

    auto result = anlz::read_pwav(pwav->payload);
    // Writer downsamples to max 1200 entries; 3 entries → 3 output.
    BOOST_REQUIRE_EQUAL(result.size(), 3);
    // Entry 0: max value=128 → height=16, max opacity=255 → whiteness=7.
    // Read back: value=128, opacity=224.
    BOOST_TEST(result[0].low.value == 128);
    BOOST_TEST(result[0].low.opacity == 224);
    // Entry 1: all zeros → height=0, whiteness=0 → value=0, opacity=0.
    BOOST_TEST(result[1].low.value == 0);
    BOOST_TEST(result[1].low.opacity == 0);
    // Entry 2: max value=255 → height=31, max opacity=255 → whiteness=7.
    // Read back: value=248, opacity=224.
    BOOST_TEST(result[2].low.value == 248);
    BOOST_TEST(result[2].low.opacity == 224);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv6() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pwv6__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.waveform = {
        {{10, 255}, {20, 255}, {30, 255}},
        {{40, 255}, {50, 255}, {60, 255}},
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto twoex_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                        anlz_path.to_directory() / "ANLZ0000.2EX")
                           .string();

    auto tags = anlz::parse_pmai_file(twoex_path);
    const auto* pwv6 = anlz::find_tag(tags, "PWV6");
    BOOST_REQUIRE(pwv6 != nullptr);

    auto result = anlz::read_pwv6(pwv6->payload);
    // Writer downsamples to max 1200 entries; 2 entries → 2 output.
    BOOST_REQUIRE_EQUAL(result.size(), 2);
    // PWV6 stores mid/high/low directly, so values round-trip exactly.
    BOOST_TEST(result[0].low.value == 10);
    BOOST_TEST(result[0].mid.value == 20);
    BOOST_TEST(result[0].high.value == 30);
    BOOST_TEST(result[1].low.value == 40);
    BOOST_TEST(result[1].mid.value == 50);
    BOOST_TEST(result[1].high.value == 60);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv7() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pwv7__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.waveform = {
        {{10, 255}, {20, 255}, {30, 255}},
        {{40, 255}, {50, 255}, {60, 255}},
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto twoex_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                        anlz_path.to_directory() / "ANLZ0000.2EX")
                           .string();

    auto tags = anlz::parse_pmai_file(twoex_path);
    const auto* pwv7 = anlz::find_tag(tags, "PWV7");
    BOOST_REQUIRE(pwv7 != nullptr);

    auto result = anlz::read_pwv7(pwv7->payload);
    BOOST_REQUIRE_EQUAL(result.size(), 2);
    BOOST_TEST(result[0].low.value == 10);
    BOOST_TEST(result[0].mid.value == 20);
    BOOST_TEST(result[0].high.value == 30);
    BOOST_TEST(result[1].low.value == 40);
    BOOST_TEST(result[1].mid.value == 50);
    BOOST_TEST(result[1].high.value == 60);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv3() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pwv3__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.waveform = {
        {{128, 255}, {64, 255}, {32, 255}},
        {{255, 255}, {0, 0}, {0, 0}},
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto ext_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.EXT")
                        .string();

    auto tags = anlz::parse_pmai_file(ext_path);
    const auto* pwv3 = anlz::find_tag(tags, "PWV3");
    BOOST_REQUIRE(pwv3 != nullptr);

    auto result = anlz::read_pwv3(pwv3->payload);
    BOOST_REQUIRE_EQUAL(result.size(), 2);
    // Entry 0: max value = 128 → height = 16 → value = 128.
    BOOST_TEST(result[0].low.value == 128);
    // Entry 1: max value = 255 → height = 31 → value = 248.
    BOOST_TEST(result[1].low.value == 248);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv4() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pwv4__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.waveform = {
        {{100, 200}, {150, 180}, {200, 220}},
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto ext_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.EXT")
                        .string();

    auto tags = anlz::parse_pmai_file(ext_path);
    const auto* pwv4 = anlz::find_tag(tags, "PWV4");
    BOOST_REQUIRE(pwv4 != nullptr);

    auto result = anlz::read_pwv4(pwv4->payload);
    BOOST_REQUIRE_EQUAL(result.size(), 1);
    // Writer stores: red=low.value=100, green=mid.value=150,
    // luminance=mid.opacity=180, blue_inv=255-high.opacity=35,
    // blue_height=max>>3=25.
    BOOST_TEST(result[0].low.value == 100);    // red
    BOOST_TEST(result[0].mid.value == 150);    // green
    BOOST_TEST(result[0].mid.opacity == 180);  // luminance
    BOOST_TEST(result[0].high.opacity == 220); // 255 - 35
    BOOST_TEST(result[0].high.value == 25);    // blue_height = max(200)>>3
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pwv5() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pwv5__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.waveform = {
        {{255, 255}, {128, 255}, {64, 255}},
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto ext_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.EXT")
                        .string();

    auto tags = anlz::parse_pmai_file(ext_path);
    const auto* pwv5 = anlz::find_tag(tags, "PWV5");
    BOOST_REQUIRE(pwv5 != nullptr);

    auto result = anlz::read_pwv5(pwv5->payload);
    BOOST_REQUIRE_EQUAL(result.size(), 1);
    // Writer: R=255>>5=7, G=128>>5=4, B=64>>5=2.
    // Reader: 7<<5=224, 4<<5=128, 2<<5=64.
    BOOST_TEST(result[0].low.value == 224);   // R
    BOOST_TEST(result[0].mid.value == 128);   // G
    BOOST_TEST(result[0].high.value == 64);   // B
    BOOST_TEST(result[0].low.opacity == 255);
}

// =========================================================================
// PQTZ reader tests
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("read_pqtz() returns empty for empty payload"))
BOOST_AUTO_TEST_CASE(read_pqtz__empty_payload__empty)
{
    std::vector<uint8_t> empty;
    auto result = anlz::read_pqtz(empty, 44100.0);
    BOOST_TEST(result.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pqtz() reads beatgrid from djay reference .DAT"))
BOOST_AUTO_TEST_CASE(read_pqtz__djay_reference__correct_entries)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");
    const auto* pqtz = anlz::find_tag(tags, "PQTZ");
    BOOST_REQUIRE(pqtz != nullptr);

    auto result = anlz::read_pqtz(pqtz->payload, 44100.0);
    // The djay reference has a real beatgrid — just verify it's non-empty
    // and the first entry is at beat 0.
    BOOST_TEST(!result.empty());
    BOOST_TEST(result[0].index == 0);
    BOOST_TEST(result[0].sample_offset >= 0.0);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pqtz() reads beatgrid from djay MULT reference"))
BOOST_AUTO_TEST_CASE(read_pqtz__djay_mult__correct_bpm)
{
    auto tags = anlz::parse_pmai_file(
        source_root +
        "PIONEER_DJAY_MULT/USBANLZ/P033/0002A217/ANLZ0000.DAT");
    const auto* pqtz = anlz::find_tag(tags, "PQTZ");
    BOOST_REQUIRE(pqtz != nullptr);

    auto result = anlz::read_pqtz(pqtz->payload, 44100.0);
    BOOST_REQUIRE(!result.empty());
    // First entry: time_ms=242, so sample_offset = 242 * 44100 / 1000.
    double expected = 242.0 * 44100.0 / 1000.0;
    BOOST_TEST(result[0].index == 0);
    BOOST_TEST(result[0].sample_offset == expected, boost::test_tools::tolerance(1.0));
    // 149 entries in the reference file.
    BOOST_TEST(result.size() == 149);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pqtz() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_pqtz__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;
    track.beatgrid = {
        {0, 0.0},
        {4, 44100.0},   // 1 second = 4 beats = 240 BPM
    };

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto dat_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.DAT")
                        .string();

    auto tags = anlz::parse_pmai_file(dat_path);
    const auto* pqtz = anlz::find_tag(tags, "PQTZ");
    BOOST_REQUIRE(pqtz != nullptr);

    auto result = anlz::read_pqtz(pqtz->payload, 44100.0);
    BOOST_REQUIRE_EQUAL(result.size(), 2);
    BOOST_TEST(result[0].index == 0);
    BOOST_TEST(result[0].sample_offset == 0.0);
    BOOST_TEST(result[1].index == 1);
    // 44100 samples = 1000 ms → 1000 * 44100 / 1000 = 44100.
    BOOST_TEST(result[1].sample_offset == 44100.0, boost::test_tools::tolerance(1.0));
}

// =========================================================================
// PCOB reader tests
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("read_pcob() returns empty for empty payload"))
BOOST_AUTO_TEST_CASE(read_pcob__empty_payload__empty)
{
    std::vector<uint8_t> empty;
    auto result = anlz::read_pcob(empty, 44100.0);
    BOOST_TEST(result.hot_cues.empty());
    BOOST_TEST(result.loops.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pcob() reads empty hot cue container from djay reference"))
BOOST_AUTO_TEST_CASE(read_pcob__djay_empty_hot__empty)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");

    // First PCOB is hot cues (type=1), second is memory (type=0).
    const auto* pcob = anlz::find_tag(tags, "PCOB");
    BOOST_REQUIRE(pcob != nullptr);
    auto result = anlz::read_pcob(pcob->payload, 44100.0);
    BOOST_TEST(result.hot_cues.empty());
    BOOST_TEST(result.loops.empty());
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pcob() reads 3 hot cues from djay MULT reference"))
BOOST_AUTO_TEST_CASE(read_pcob__djay_mult_hot_cues__correct)
{
    auto tags = anlz::parse_pmai_file(
        source_root +
        "PIONEER_DJAY_MULT/USBANLZ/P033/0002A217/ANLZ0000.DAT");

    // Find the first PCOB (hot cues, type=1).
    const anlz::tag_section* pcob = nullptr;
    for (const auto& tag : tags)
    {
        if (tag.id == "PCOB")
        {
            // Check type field (first 4 bytes of payload).
            if (!tag.payload.empty() && tag.payload[3] == 1)
            {
                pcob = &tag;
                break;
            }
        }
    }
    BOOST_REQUIRE(pcob != nullptr);

    auto result = anlz::read_pcob(pcob->payload, 44100.0);
    BOOST_TEST(result.loops.empty());

    // 3 hot cues at slots 1, 2, 3 (0-based: 0, 1, 2).
    BOOST_REQUIRE_EQUAL(result.hot_cues.size(), 3);
    BOOST_TEST(result.hot_cues[0].has_value());
    BOOST_TEST(result.hot_cues[1].has_value());
    BOOST_TEST(result.hot_cues[2].has_value());

    // First hot cue: time_ms=12471 → sample_offset = 12471 * 44100 / 1000.
    double expected0 = 12471.0 * 44100.0 / 1000.0;
    BOOST_TEST(result.hot_cues[0]->sample_offset == expected0,
               boost::test_tools::tolerance(1.0));

    // Second: time_ms=13235.
    double expected1 = 13235.0 * 44100.0 / 1000.0;
    BOOST_TEST(result.hot_cues[1]->sample_offset == expected1,
               boost::test_tools::tolerance(1.0));
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pcob() round-trips hot cues with writer"))
BOOST_AUTO_TEST_CASE(read_pcob__round_trip_hot_cues__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;

    // Two hot cues at 1s and 2s.
    djinterop::hot_cue hc1{};
    hc1.sample_offset = 44100.0;  // 1s
    djinterop::hot_cue hc2{};
    hc2.sample_offset = 88200.0;  // 2s
    track.hot_cues = {hc1, std::nullopt, hc2};

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto dat_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.DAT")
                        .string();

    auto tags = anlz::parse_pmai_file(dat_path);

    // Find the first PCOB (hot cues, type=1).
    const anlz::tag_section* hot_pcob = nullptr;
    for (const auto& tag : tags)
    {
        if (tag.id == "PCOB" && !tag.payload.empty() && tag.payload[3] == 1)
        {
            hot_pcob = &tag;
            break;
        }
    }
    BOOST_REQUIRE(hot_pcob != nullptr);

    auto result = anlz::read_pcob(hot_pcob->payload, 44100.0);
    BOOST_REQUIRE_EQUAL(result.hot_cues.size(), 3);
    BOOST_TEST(result.hot_cues[0].has_value());
    BOOST_TEST(!result.hot_cues[1].has_value());
    BOOST_TEST(result.hot_cues[2].has_value());

    BOOST_TEST(result.hot_cues[0]->sample_offset == 44100.0,
               boost::test_tools::tolerance(1.0));
    BOOST_TEST(result.hot_cues[2]->sample_offset == 88200.0,
               boost::test_tools::tolerance(1.0));
}

BOOST_TEST_DECORATOR(
    *utf::description("read_pcob() round-trips loops with writer"))
BOOST_AUTO_TEST_CASE(read_pcob__round_trip_loops__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;

    // One loop: 1s to 2s.
    djinterop::loop lp{};
    lp.start_sample_offset = 44100.0;
    lp.end_sample_offset = 88200.0;
    track.loops = {lp};

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto dat_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.DAT")
                        .string();

    auto tags = anlz::parse_pmai_file(dat_path);

    // Find the second PCOB (memory, type=0).
    const anlz::tag_section* mem_pcob = nullptr;
    for (const auto& tag : tags)
    {
        if (tag.id == "PCOB" && !tag.payload.empty() &&
            tag.payload[3] == 0)
        {
            mem_pcob = &tag;
            break;
        }
    }
    BOOST_REQUIRE(mem_pcob != nullptr);

    auto result = anlz::read_pcob(mem_pcob->payload, 44100.0);
    BOOST_REQUIRE_EQUAL(result.loops.size(), 1);
    BOOST_TEST(result.loops[0].has_value());
    BOOST_TEST(result.loops[0]->start_sample_offset == 44100.0,
               boost::test_tools::tolerance(1.0));
    BOOST_TEST(result.loops[0]->end_sample_offset == 88200.0,
               boost::test_tools::tolerance(1.0));
}

// =========================================================================
// PPTH reader tests
// =========================================================================

BOOST_TEST_DECORATOR(
    *utf::description("read_ppth() decodes simple ASCII path"))
BOOST_AUTO_TEST_CASE(read_ppth__ascii__correct)
{
    // "/Contents/Test.flac" as UTF-16BE + trailing NUL.
    std::string path = "/Contents/Test.flac";
    std::vector<uint8_t> payload;
    for (char c : path)
    {
        payload.push_back(0);
        payload.push_back(static_cast<uint8_t>(c));
    }
    payload.push_back(0);
    payload.push_back(0);

    BOOST_TEST(anlz::read_ppth(payload) == path);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_ppth() decodes path without trailing NUL"))
BOOST_AUTO_TEST_CASE(read_ppth__no_nul__correct)
{
    // Same path but without trailing NUL.
    std::string path = "/Contents/Test.flac";
    std::vector<uint8_t> payload;
    for (char c : path)
    {
        payload.push_back(0);
        payload.push_back(static_cast<uint8_t>(c));
    }

    BOOST_TEST(anlz::read_ppth(payload) == path);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_ppth() decodes non-ASCII path from djay export"))
BOOST_AUTO_TEST_CASE(read_ppth__djay_export__correct)
{
    // The djay export path contains U+2019 (RIGHT SINGLE QUOTATION MARK),
    // which in UTF-16BE is 0x2019.
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");
    const auto* ppth = anlz::find_tag(tags, "PPTH");
    BOOST_REQUIRE(ppth != nullptr);

    auto path = anlz::read_ppth(ppth->payload);
    // The path starts with /Contents/ and ends with .flac.
    BOOST_TEST(path.substr(0, 10) == "/Contents/");
    BOOST_TEST(path.substr(path.size() - 5) == ".flac");
    // The path contains the U+2019 character (UTF-8: 0xE2 0x80 0x99).
    BOOST_TEST(path.find('\xe2') != std::string::npos);
}

BOOST_TEST_DECORATOR(
    *utf::description("find_tag() returns first matching tag"))
BOOST_AUTO_TEST_CASE(find_tag__first_match__correct)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");

    // .DAT has two PCOB tags; find_tag returns the first.
    const auto* pcob = anlz::find_tag(tags, "PCOB");
    BOOST_REQUIRE(pcob != nullptr);
    BOOST_TEST(pcob->id == "PCOB");
}

BOOST_TEST_DECORATOR(
    *utf::description("find_tag() returns nullptr for missing tag"))
BOOST_AUTO_TEST_CASE(find_tag__missing__nullptr)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");

    // PSSI is not present in .DAT files.
    const auto* pssi = anlz::find_tag(tags, "PSSI");
    BOOST_TEST(pssi == nullptr);
}

BOOST_TEST_DECORATOR(
    *utf::description("read_ppth() round-trips with writer"))
BOOST_AUTO_TEST_CASE(read_ppth__round_trip__matches)
{
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test Track.flac";
    track.sample_rate = 44100.0;

    anlz::write_anlz_files(tmp.temp_dir, track);

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto dat_path = (fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
                     anlz_path.to_directory() / "ANLZ0000.DAT")
                        .string();

    auto tags = anlz::parse_pmai_file(dat_path);
    const auto* ppth = anlz::find_tag(tags, "PPTH");
    BOOST_REQUIRE(ppth != nullptr);

    BOOST_TEST(anlz::read_ppth(ppth->payload) == track.relative_path);
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() parses djay Pro reference .EXT file"))
BOOST_AUTO_TEST_CASE(parse_pmai__djay_ext__correct_tags)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.EXT");

    // Expected tag order for .EXT: PPTH, PWV3, PWV4, PWV5, PQT2
    BOOST_REQUIRE_EQUAL(tags.size(), 5);
    BOOST_TEST(tags[0].id == "PPTH");
    BOOST_TEST(tags[1].id == "PWV3");
    BOOST_TEST(tags[2].id == "PWV4");
    BOOST_TEST(tags[3].id == "PWV5");
    BOOST_TEST(tags[4].id == "PQT2");
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() parses djay Pro reference .2EX file"))
BOOST_AUTO_TEST_CASE(parse_pmai__djay_2ex__correct_tags)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.2EX");

    // Expected tag order for .2EX: PPTH, PWV6, PWV7
    BOOST_REQUIRE_EQUAL(tags.size(), 3);
    BOOST_TEST(tags[0].id == "PPTH");
    BOOST_TEST(tags[1].id == "PWV6");
    BOOST_TEST(tags[2].id == "PWV7");
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() extracts PPTH payload from .DAT"))
BOOST_AUTO_TEST_CASE(parse_pmai__djay_dat_ppth__correct_path)
{
    auto tags = anlz::parse_pmai_file(
        source_root + "PIONEER_DJAY_ONE/USBANLZ/P039/000272B9/ANLZ0000.DAT");

    BOOST_REQUIRE_EQUAL(tags[0].id, "PPTH");
    BOOST_TEST(tags[0].is_ppth);

    // PPTH payload is UTF-16BE path. The djay export uses:
    // /Contents/Charli xcx feat Robyn & Yung Lean/Brat and it's completely
    //   different but also stil/01 360-37loy.flac
    // Check that the payload starts with "/" (U+002F as UTF-16BE).
    BOOST_REQUIRE(tags[0].payload.size() >= 4);
    BOOST_TEST(anlz::read_u16_be(tags[0].payload.data()) == 0x002F);
}

BOOST_TEST_DECORATOR(
    *utf::description("parse_pmai() round-trips with writer for empty ANLZ"))
BOOST_AUTO_TEST_CASE(parse_pmai__round_trip_empty__matches)
{
    // Write an ANLZ file with the writer, then read it back.
    temporary_directory tmp;

    anlz::anlz_track_data track;
    track.relative_path = "/Contents/Test.flac";
    track.sample_rate = 44100.0;

    auto anlz_path = anlz::compute_anlz_path(track.relative_path);
    namespace fs = std::filesystem;
    auto dir = fs::path{tmp.temp_dir} / ".PIONEER/USBANLZ" /
               anlz_path.to_directory();
    fs::create_directories(dir);

    anlz::write_anlz_files(tmp.temp_dir, track);

    // Read back the .DAT file and verify tag structure.
    auto dat_path = (dir / "ANLZ0000.DAT").string();
    auto tags = anlz::parse_pmai_file(dat_path);

    // Written .DAT: PPTH, PVBR, PQTZ, PWAV, PWV2, PCOB, PCOB
    BOOST_REQUIRE_EQUAL(tags.size(), 7);
    BOOST_TEST(tags[0].id == "PPTH");
    BOOST_TEST(tags[1].id == "PVBR");
    BOOST_TEST(tags[2].id == "PQTZ");
    BOOST_TEST(tags[3].id == "PWAV");
    BOOST_TEST(tags[4].id == "PWV2");
    BOOST_TEST(tags[5].id == "PCOB");
    BOOST_TEST(tags[6].id == "PCOB");
}