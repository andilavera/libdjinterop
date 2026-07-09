# OneLibrary (Device Library Plus) — Outstanding Work

This document tracks what is needed for **full** OneLibrary / AlphaTheta Device
Library Plus support in libdjinterop, as of the current working state
(commits through `42cdf0b` + unstaged bridge implementation).

Status legend:
- [x] Done
- [~] Partial / stubbed
- [ ] Not started

---

## Reference Materials

These external and internal sources document the formats and integration
patterns. Items marked `[read]` should be read for understanding; items marked
`[model]` are implementations to follow or adapt.

### Format Specifications & Binary Definitions

| Ref | Source | What It Covers |
|-----|--------|----------------|
| [R1] | `SPEC.md` | Canonical OneLibrary DB schema, ANLZ layout, encryption, data conventions |
| [R2] | `tmp/crate-digger/src/main/kaitai/rekordbox_anlz.ksy` | **[model]** Kaitai Struct definition of the ANLZ binary format — all tag types, field sizes, endianness. Defines `beat_grid_tag` (PQTZ), `cue_tag` (PCOB), `cue_extended_tag` (PCO2), `wave_preview_tag` (PWAV), `wave_scroll_tag` (PWV3), `wave_color_preview_tag` (PWV4), `wave_color_scroll_tag` (PWV5), `wave_3band_preview_tag` (PWV6), `wave_3band_scroll_tag` (PWV7), `song_structure_tag` (PSSI) |
| [R3] | `tmp/crate-digger/doc/modules/ROOT/pages/anlz.adoc` | **[read]** Detailed byte-level documentation of every ANLZ tag: PQTZ beat grid entries (8 bytes: beat_number u2, tempo u2×100, time_ms u4), PCOB cue entries (38 bytes with `PCPT` sub-tag, hot_cue number, status, type=1 point/2 loop, time_ms, loop_time_ms), PCO2 extended cues (variable-length `PCP2` sub-tag with color_id, loop_numerator/denominator, UTF-16BE comment, RGB color), PSSI song structure with XOR unmasking, waveform tag entry formats |
| [R4] | `tmp/pyrekordbox/docs/source/formats/anlz.md` | **[read]** pyrekordbox's ANLZ format documentation — tag order conventions per file type (DAT: PPTH,PVBR,PQTZ,PWAV,PWV2,PCOB,PCOB / EXT: PPTH,PCOB,PCOB,PCO2,PCO2,PQT2,PWV3,PWV4,PWV5,PSSI / 2EX: PPTH,PWV6,PWV7,PWVC) |
| [R5] | `tmp/pyrekordbox/docs/source/formats/devicelib_plus.md` | **[read]** pyrekordbox's Device Library Plus documentation |

### Reference Implementations — ANLZ Parsing

| Ref | Source | What It Covers |
|-----|--------|----------------|
| [R6] | `tmp/pyrekordbox/pyrekordbox/anlz/file.py` | **[model]** Complete ANLZ file parser: `AnlzFile.parse()` reads PMAI header, iterates tagged sections by `len_tag`, dispatches to tag handlers via `TAGS` dict, handles PSSI XOR unmasking (lines 90-140 — the XOR key is `CB E1 EE FA E5 EE AD EE E9 D2 E9 EB E1 E9 F3 E8 E9 F4 E1`, added to `len_entries` per byte, modulo 256) |
| [R7] | `tmp/pyrekordbox/pyrekordbox/anlz/tags.py` | **[model]** Per-tag handler classes: `PQTZAnlzTag` — parses beat grid entries, `get()` returns `(beats[], bpms[], times_ms[])`; `PCOBAnlzTag` — wraps cue list; `PCO2AnlzTag` — wraps extended cue list; `PPTHAnlzTag` — path get/set; `PVBRAnlzTag` — VBR index; `PSSIAnlzTag` — song structure; `PWAVAnlzTag`/`PWV2AnlzTag` — mono waveform (1 byte/entry, height in low 5 bits, whiteness in high 3 bits); `PWV3AnlzTag` — same encoding as PWAV; `PWV4AnlzTag` — 6-byte color preview entries; `PWV5AnlzTag` — 2-byte color detail (3-bit R/G/B, 5-bit height); `PWV6AnlzTag`/`PWV7AnlzTag` — 3-byte 3-band entries (mid/high/low order); `PWVCAnlzTag` — color waveform (seen in 2EX). **Tag registry at end of file:** maps fourcc → handler class |
| [R8] | `tmp/pyrekordbox/pyrekordbox/anlz/structs.py` | **[model]** Binary struct definitions using Python `construct`: `AnlzTag` (4cc + len_header + len_tag + body), `AnlzFileHeader` (PMAI + len_header + len_file), `PQTZ` (pad4 + 0x80000 + entry_count + entries of `AnlzQuantizeTick{beat:u2, tempo:u2, time_ms:u4}`), `PCOB` (cue_type:u4 + unk:u2 + count:u2 + memory_count:s4 + entries of `AnlzCuePoint`), `PCO2` (type:u4 + count:u2 + unknown:u2 + entries of `AnlzCuePoint2` with color_id, comment, RGB), `PPTH` (len_path:u4 + UTF-16BE path), `PQT2` (pad4 + u1 + pad4 + 2×QuantizeTick + entry_count + u3 + u4 + u5 + 2-byte entries), waveform structs (PWAV/PWV2: len_preview + 0x10000 + bytes; PWV3: 1 + len_entries + 0x00960000 + bytes; PWV5: 2 + len_entries + unknown + u2 entries), `PSSI` (len_entry_bytes + len_entries + mood + 6 unknown + end_beat + 2 unknown + bank + 1 unknown + SongStructureEntry{index,beat,kind,k1,k2,b,beat2-4,k3,fill,beat_fill}) |

### Reference Implementations — Device Library Plus Database

| Ref | Source | What It Covers |
|-----|--------|----------------|
| [R9] | `tmp/pyrekordbox/pyrekordbox/devicelib_plus/models.py` | **[model]** SQLAlchemy ORM for all 22 tables. Key: `Content` model (lines ~200-400) maps every column including `artist_id_artist`, `artist_id_remixer`, `artist_id_originalArtist`, `artist_id_composer`, `artist_id_lyricist`, `album_id`, `genre_id`, `label_id`, `key_id`, `color_id`, `image_id` with relationship proxies (`artist_name`, `remixer_name`, `album_name`, `genre_name`, etc.). Also: `Cue` model with all position columns (see §5.1); `Playlist`/`PlaylistContent`; `History`/`HistoryContent`; `MyTag`/`MyTagContent`; `HotCueBankList`/`HotCueBankListCue` |
| [R10] | `tmp/pyrekordbox/pyrekordbox/devicelib_plus/database.py` | **[read]** SQLCipher open with key deobfuscation, session management, `add_artist()`, `add_album()`, `add_genre()` helper methods — reference for the add-or-resolve pattern |

### Reference Implementations — Mixxx Integration

| Ref | Source | What It Covers |
|-----|--------|----------------|
| [R11] | `tmp/mixxx/src/library/export/engineprimeexportjob.cpp` | **[model]** How Mixxx maps its data to `djinterop::track_snapshot`. Lines 200-320: beatgrid via `e::normalize_beatgrid()`, hot cues → `snapshot.hot_cues[index]` with `djinterop::hot_cue{label, sample_offset, color}`, loops → `snapshot.loops[index]` with `djinterop::loop{label, start_sample_offset, end_sample_offset, color}`, waveform via `djinterop::waveform_entry{{low, opacity}, {mid, opacity}, {high, opacity}}`, key via `toDjinteropKey()` mapping table, main_cue, average_loudness, rating (0-100 scale). Lines 120-165: `tryGetBeatgrid()` helper — iterates Mixxx beat iterator, builds `beatgrid_marker` vector with `sample_offset` and `bpm` fields |
| [R12] | `tmp/mixxx/src/library/export/engineprimeexportrequest.h` | **[read]** Request struct pattern for export jobs: `engineLibraryDbDir`, `musicFilesDir`, `exportSchemaVersion`, `crateIdsToExport`, `playlistIdsToExport` |
| [R13] | `tmp/mixxx/src/library/export/engineprimeexportjob.h` | **[read]** QThread-based export job class declaration with signals: `jobMaximum`, `jobProgress`, `completed`, `failed` |

### Our Own Implementation (Existing)

| Ref | Source | What It Covers |
|-----|--------|----------------|
| [R14] | `src/djinterop/onelibrary/anlz/anlz_writer.cpp` | Our ANLZ writer: `write_anlz_files()` builds all 3 files. Currently uses synthetic data — see §2.3 |
| [R15] | `src/djinterop/onelibrary/anlz/tag_writers.hpp` | Our tag payload builders: `build_pqtz_payload()`, `build_pcob_payload()`, `build_empty_pcob_payload()`, `build_pwav_payload()`, `build_pwv2_payload()` through `build_pwv7_payload()`, `build_empty_pqt2_payload()` |
| [R16] | `src/djinterop/onelibrary/anlz/pmai_writer.hpp` | Our PMAI container writer: `write_pmai_file()`, `serialize_tag()` |
| [R17] | `src/djinterop/onelibrary/anlz/hash.hpp` | Our ANLZ path hash: `compute_anlz_path()` |
| [R18] | `src/djinterop/onelibrary/track_impl.cpp` | Our bridge implementation — shows current gaps (§2.2, §3) |
| [R19] | `src/djinterop/engine/v3/track_impl.cpp` | Reference for a "complete" track_impl: how Engine Prime v3 handles beatgrid, waveform, hot_cues, loops, key in the bridge |
| [R20] | `src/djinterop/impl/track_impl.hpp` | Base class interface contract that our OneLibrary `track_impl` must fulfill |

---

## 1. Architecture Overview

The implementation has three layers:

```
┌──────────────────────────────────────────────────────┐
│  High-level public API                                │
│  djinterop::database / track / playlist / crate       │
│  (polymorphic, same interface as Engine Prime)        │
├──────────────────────────────────────────────────────┤
│  Bridge layer (database_impl / track_impl /           │
│  playlist_impl) — translates generic API to           │
│  OneLibrary-specific operations                       │
├──────────────────────────────────────────────────────┤
│  Low-level layer (onelibrary class, *_table classes,  │
│  schema, ANLZ writers) — direct DB/ANLZ I/O           │
└──────────────────────────────────────────────────────┘
```

**Current state:** The bridge layer exists for `database_impl`, `track_impl`,
and `playlist_impl` [R18]. The `onelibrary_factory` free functions
(`create_database`, `load_database`, etc.) connect the bridge to the low-level
layer. The low-level layer has full schema creation, default catalogues, CRUD
for tracks/playlists/artwork/reference tables, and ANLZ *writing* support
[R14][R15][R16].

---

## 2. ANLZ Read/Write Bridge (Tier 1 — Critical for Mixxx)

These fields in `track_snapshot` are stored in ANLZ sidecar files, **not** in
`exportLibrary.db`. The `track_impl` bridge [R18] currently returns empty/null
for all of them. An **ANLZ reader** is needed to populate them, and the
existing ANLZ writer [R14][R15] needs to accept real data instead of synthetic
filler.

> **Why this matters for Mixxx:** Mixxx's `EnginePrimeExportJob` [R11 lines
> 200-320] maps hot cues, loops, beatgrid markers, waveform entries, musical
> key, main cue, and average loudness into `track_snapshot` before calling
> `pDb->create_track(snapshot)` or `track->update(snapshot)`. For OneLibrary,
> these fields must be persisted to ANLZ files, not the database. Without the
> ANLZ bridge, a Mixxx OneLibrary export would silently drop all performance
> data.

### 2.1 ANLZ Reader

The ANLZ format is a PMAI container with tagged sections. The binary structure
is defined in the Kaitai spec [R2] and documented at byte level in
crate-digger's docs [R3]. pyrekordbox has a complete Python reader [R6][R7][R8]
that serves as the reference implementation.

- [ ] **Implement ANLZ file parser** — read `.DAT`, `.EXT`, `.2EX` PMAI
  containers, parse tagged sections, and extract structured data.
  - **Binary format:** [R2] Kaitai struct — PMAI magic (4 bytes) + `len_header`
    (u4) + `len_file` (u4) + tagged sections until EOF. Each section: fourcc
    (4 bytes) + `len_header` (u4) + `len_tag` (u4) + body (`len_tag - 12`
    bytes). All integers big-endian.
  - **Reference parser:** [R6] `AnlzFile._parse()` — iterates sections by
    advancing `i += len_tag`, dispatches on fourcc via `TAGS` dict. Also
    handles PSSI XOR unmasking (see §4.1).
  - **Known tags per file type** ([R4]): `.DAT` = PPTH, PVBR, PQTZ, PWAV,
    PWV2, PCOB (×2: hot + memory). `.EXT` = PPTH, PCOB (×2), PCO2 (×2),
    PQT2, PWV3, PWV4, PWV5, PSSI. `.2EX` = PPTH, PWV6, PWV7, PWVC.

- [ ] **PPTH** — read and validate the volume-relative audio path.
  - **Binary:** [R2] `path_tag`: `len_path` (u4) + UTF-16BE string with
    trailing NUL.
  - **Reference:** [R7] `PPTHAnlzTag` — `get()` returns path string.
  - Already written correctly by our ANLZ writer [R14].

- [ ] **PCOB (cues)** — parse memory cues and hot cues with timestamps and
  loop flags. Map to `track_snapshot::hot_cues` and `track_snapshot::loops`.
  - **Binary:** [R2] `cue_tag` and `cue_entry`: type (u4, 0=memory/1=hot) +
    unk (2 bytes) + num_cues (u2) + memory_count (u4) + entries. Each entry
    is a `PCPT` sub-tag (38 bytes): hot_cue number (u4, 0=memory), status
    (u4, 4=active loop), type (u1, 1=point/2=loop), time_ms (u4),
    loop_time_ms (u4), 16 bytes padding.
  - **Reference:** [R3] "Cue List Tag" section — full byte-field diagrams of
    the PCOB tag and PCPT entry. [R8] `PCOB` struct and `AnlzCuePoint` struct
    definitions. [R7] `PCOBAnlzTag` handler.
  - **Mapping to djinterop:** [R11 lines 238-293] shows how Mixxx maps: hot
    cues → `djinterop::hot_cue{sample_offset, label, color}`, loops →
    `djinterop::loop{start_sample_offset, end_sample_offset, label, color}`.
    Time_ms from ANLZ must be converted to sample offsets using the track's
    sample rate (`sample_offset = time_ms * sample_rate / 1000`). Loop:
    `start_sample_offset` = time_ms converted, `end_sample_offset` =
    loop_time_ms converted.

- [ ] **PQTZ (beatgrid)** — parse beat grid markers, map to
  `track_snapshot::beatgrid`.
  - **Binary:** [R2] `beat_grid_tag`: pad(4) + 0x80000 (u4) + num_beats (u4)
    + entries. Each entry = 8 bytes: beat_number (u2, 1-4), tempo (u2, BPM×100),
    time_ms (u4).
  - **Reference:** [R3] "Beat Grid Tag" section. [R8] `PQTZ` struct and
    `AnlzQuantizeTick` struct. [R7] `PQTZAnlzTag` — `get()` returns
    `(beats[], bpms[], times[])`.
  - **Mapping to djinterop:** [R11 lines 120-165] `tryGetBeatgrid()` creates
    `beatgrid_marker` entries with `sample_offset` and `bpm`. Convert ANLZ
    `time_ms` → sample offset. Use `bpm = tempo / 100.0`.

- [ ] **PWAV / PWV2 / PWV3 / PWV4 / PWV5 / PWV6 / PWV7 (waveforms)** — parse
  waveform entries, map to `track_snapshot::waveform`.
  - **Binary per tag:**
    - **PWAV** (mono overview, 400 bytes): [R8] `PWAV` struct — len_preview
      (u4) + 0x10000 (u4) + bytes. Each byte: height in low 5 bits (0-31),
      whiteness in high 3 bits. [R3] "Waveform Preview Tag".
    - **PWV2** (tiny mono, 100 bytes): Same struct as PWAV. Height in low 4
      bits (0-15). [R3] "Tiny Waveform Preview Tag".
    - **PWV3** (mono detail, ~150/sec): [R8] `PWV3` struct — 1 (u4) +
      len_entries (u4) + 0x00960000 (u4) + bytes. Same byte encoding as PWAV.
    - **PWV4** (color preview, 1200×6 bytes): [R8] `PWV4` struct — 6 (u4) +
      len_entries (u4) + unknown (u4) + raw bytes. Each 6-byte entry: [R7]
      `PWV4AnlzTag.get()` parses heights + RGB colors. See [R3] "Waveform
      Color Preview Tag".
    - **PWV5** (color detail, ~150/sec×2 bytes): [R8] `PWV5` struct — 2 (u4)
      + len_entries (u4) + unknown (u4) + u2 entries. Each 2-byte entry:
      3-bit R/G/B + 5-bit height + 2 unused bits. [R3] "Waveform Color Detail
      Tag" has bit diagram. [R7] `PWV5AnlzTag.get()` parses to heights and
      RGB colors.
    - **PWV6** (3-band preview, 1200×3 bytes): [R8] 3 (u4) + len_entries (u4)
      + 3-byte entries (mid, high, low order). [R3] "Waveform 3-Band Preview
      Tag".
    - **PWV7** (3-band detail, ~150/sec×3 bytes): [R8] same as PWV3 header
      + 3-byte entries. [R3] "Waveform 3-Band Detail Tag".
  - **Mapping to djinterop:** [R11 lines 297-313] creates
    `waveform_entry{{low, opacity}, {mid, opacity}, {high, opacity}}`. For
    mono waveforms, duplicate the single height value to all 3 bands. For
    color preview/detail, extract the green channel as a rough amplitude
    proxy. For 3-band, use mid/high/low directly. Opacity defaults to 127
    (`kDefaultWaveformOpacity` in R11).

- [ ] **PCO2** — extended cue data (nxs2 format with labels, colours).
  - **Binary:** [R2] `cue_extended_tag`: type (u4) + num_cues (u2) + 2
    unknown + entries. Each entry is a `PCP2` sub-tag (variable length):
    hot_cue (u4), type (u1, 1=point/2=loop), 3 unknown, time_ms (u4),
    loop_time_ms (u4), color_id (u1), 7 unknown, loop_numerator (u2),
    loop_denominator (u2), len_comment (u4), UTF-16BE comment, color_code
    (u1), color_r/g/b (3×u1).
  - **Reference:** [R3] "Extended (nxs2) Cue List Tag" — full byte-field
    diagram. [R8] `PCO2` struct and `AnlzCuePoint2` struct. [R7]
    `PCO2AnlzTag` handler.
  - **Strategy:** Prefer PCO2 over PCOB when both are present (PCO2 has
    richer data: labels, colours, quantized loop info).

- [ ] **PQT2** — extended beat grid (nxs2 format).
  - **Binary:** [R8] `PQT2` struct — pad(4) + u1(u4, version?) + pad(4) +
    2×`AnlzQuantizeTick` (16 bytes) + entry_count (u4) + u3/u4/u5 (3×u4) +
    2-byte entries.
  - **Strategy:** Prefer PQT2 when present. Fall back to PQTZ.

- [ ] **Average loudness** — `track_snapshot::average_loudness`. Not yet
  identified in any ANLZ tag. In Engine Prime this is stored in the database.
  For OneLibrary exports, Mixxx currently exports `average_loudness = 0`
  [R11 line 214]. Research needed: check if any ANLZ tag carries this.

- [ ] **Main cue** — `track_snapshot::main_cue`. Not yet identified in ANLZ.
  Mixxx exports this as a sample offset [R11 line 219]. Research needed.

- [ ] **Last played at** — `track_snapshot::last_played_at`. Likely in history
  table or ANLZ metadata. Research needed.

### 2.2 Wire ANLZ Reader into `track_impl`

The base class contract is at [R20]. The Engine Prime v3 implementation at
[R19] shows the pattern for a complete bridge. Our current OneLibrary
`track_impl` is at [R18].

- [ ] `track_impl::snapshot()` — after reading the `content` row, also parse
  ANLZ files and merge the signal-analysis fields into the returned
  `track_snapshot`. See [R18] `to_snapshot()` and [R19] for the Engine v3
  equivalent.
- [ ] `track_impl::update()` — after updating the `content` row, also
  re-write ANLZ files with any changed signal-analysis fields.
- [ ] `track_impl::beatgrid()` — return real beatgrid from ANLZ reader.
- [ ] `track_impl::set_beatgrid()` — store and write to PQTZ/PQT2.
- [ ] `track_impl::hot_cues()` / `set_hot_cues()` — read/write PCOB hot cue
  list (type=1). See [R11 lines 270-280] for the `djinterop::hot_cue` struct
  fields Mixxx sets.
- [ ] `track_impl::hot_cue_at()` / `set_hot_cue_at()` — per-index access.
- [ ] `track_impl::loops()` / `set_loops()` — read/write PCOB memory cue list
  (type=0), filtering for loop entries (type=2 in PCPT). See [R11 lines
  282-293] for `djinterop::loop` struct fields.
- [ ] `track_impl::loop_at()` / `set_loop_at()` — per-index access.
- [ ] `track_impl::waveform()` / `set_waveform()` — read/write PWAV/PWV*
  tags. See [R11 lines 297-313] for `djinterop::waveform_entry` structure.
- [ ] `track_impl::main_cue()` / `set_main_cue()` — read/write main cue.
- [ ] `track_impl::average_loudness()` / `set_average_loudness()` — read/write.
- [ ] `track_impl::last_played_at()` / `set_last_played_at()` — read/write.

### 2.3 ANLZ Writer — Real Data Support

Our current ANLZ writer [R14][R15] generates **synthetic** waveform data
(sawtooth/zeros) and a synthetic beatgrid from a single scalar BPM. The
`anlz_track_data` struct only accepts bare `(time_ms, loop_time_ms)` pairs
for cues. This needs to be extended:

- [ ] Accept real `beatgrid_marker` vectors in `anlz_track_data`, not just a
  scalar BPM, and write proper PQTZ entries. See [R11 lines 120-165] for the
  `beatgrid_marker` struct (`sample_offset`, `bpm`). Convert sample offsets
  to time_ms for PQTZ: `time_ms = sample_offset * 1000 / sample_rate`. The
  beat_number field cycles 1-4.
- [ ] Accept real `waveform_entry` vectors and write actual PWAV/PWV*/
  PWV6/PWV7 data instead of synthetic filler. See [R11 lines 297-313] for
  the `waveform_entry` struct: `{low, mid, high}` amplitude values + opacity.
  For mono tags (PWAV/PWV2/PWV3), use the max of the 3 bands. For 3-band
  tags (PWV6/PWV7), write mid/high/low directly.
- [ ] Accept `hot_cue` and `loop` structs (with labels, colours, etc.) rather
  than bare `(time_ms, loop_time_ms)` pairs. See [R11 lines 270-293] for the
  struct shapes. Labels should be written to PCO2 (extended) tags when
  available, with PCOB (basic) as fallback.
- [ ] Accept `musical_key` and write it to the appropriate ANLZ tag (research
  needed — possibly PSSI or a yet-unidentified tag). See [R11 line 44-65]
  for the Mixxx `toDjinteropKey()` mapping table.
- [ ] Accept `average_loudness` and write to the appropriate tag (research
  needed).

---

## 3. Database Fields Not Yet in the Bridge (Tier 1)

These columns exist in the `content` table schema ([R1] §2.6.1, [R9] `Content`
model) and are populated by the low-level `add_track()` / `content_row`, but
are **not exposed** through the `track_impl` / `track_snapshot` bridge:

- [ ] **`musical_key` / `key_id`** — `track_impl::key()` returns `nullopt`,
  `set_key()` is a no-op. The `key` table exists and is seeded correctly
  ([R1] §2.6.6). `content.key_id` is set by low-level `add_track()`.
  - **Wire up:** Resolve `key_id` → key name → `musical_key` enum in
    `snapshot()`. Resolve `musical_key` → key name → `key_id` in `update()`.
  - **Reference:** [R11 lines 44-65] `toDjinteropKey()` — mapping table from
    Mixxx `ChromaticKey` (0-24) to `djinterop::musical_key` enum (C_MAJOR
    through B_MINOR). The reverse mapping is needed for read.
- [ ] **`isrc`** — `content.isrc` is set by low-level `add_track()` but
  `track_snapshot` has no `isrc` field. This requires:
  - Adding `std::optional<std::string> isrc` to `track_snapshot` at
    `include/djinterop/track_snapshot.hpp`.
  - Adding `isrc()` / `set_isrc()` to `track_impl` base class [R20] and all
    implementations (Engine v2, v3, and OneLibrary).
  - Updating `operator==` and `operator<<` for `track_snapshot`.
  - **Scope note:** cross-cutting change affecting the public API.
- [ ] **`last_played_at`** — see ANLZ section §2.1 above.
- [x] **`dj_comment` ↔ `comment`** — already wired. Verified in [R18].

### 3.1 Fields Not in `track_snapshot` (No Current Public API)

These `content` columns ([R1] §2.6.1, [R9]) have no corresponding field in
`track_snapshot` and may never need one for Mixxx's use case:

| Column | Notes |
|--------|-------|
| `subtitle` | Mix name. Not in `track_snapshot`. |
| `discNo` | Disc number. Not in `track_snapshot`. |
| `fileType` | Audio format code ([R1] §2.7.1). Not in `track_snapshot`. |
| `bitDepth` | Bit depth. Not in `track_snapshot`. |
| `color_id` | Track colour FK → `color` table ([R1] §2.6.7). Not in `track_snapshot`. |
| `image_id` | Artwork FK → `image` table ([R1] §2.6.8). Not in `track_snapshot`. |
| `djPlayCount` | Play count. Not in `track_snapshot`. |
| `isHotCueAutoLoadOn` | Boolean. Not in `track_snapshot`. |
| `masterDbId` / `masterContentId` | rekordbox linkage. rekordbox-specific. |
| `analysedBits` / `contentLink` | Magic constants 41 and 788224 ([R1] §2.4). Set correctly. |
| `analysisDataFilePath` | Path to .DAT. Set correctly by `add_track()`. |
| `*ForSearch` columns | Normalised text ([R1] §2.4). Unused by hardware. |
| `dateCreated` / `dateAdded` | Timestamps. Set on creation. |
| `hasModified` / `*UpdateCount` | Edit counters. Not needed for export. |

---

## 4. Missing ANLZ Features (Tier 2)

### 4.1 Song Structure / Phrase Analysis (PSSI Tag)

- [ ] Parse PSSI tag from `.EXT` files.
  - **Binary:** [R8] `PSSI` struct — len_entry_bytes(u4, always 24) +
    len_entries(u2) + mood(u2, 1=High/2=Mid/3=Low) + 6 unknown bytes +
    end_beat(u2) + 2 unknown + bank(u1) + 1 unknown + entries (24 bytes each).
    Each entry: index(u2), beat(u2), kind(u2), k1(u1), k2(u1), b(u1),
    beat2(u2), beat3(u2), beat4(u2), k3(u1), fill(u1), beat_fill(u2).
  - **XOR unmasking:** [R6 lines 110-140] — bytes after `len_entries` are
    XOR-ed with `XOR_MASK[x % 19] + len_entries` (mod 256). XOR_MASK =
    `CB E1 EE FA E5 EE AD EE E9 D2 E9 EB E1 E9 F3 E8 E9 F4 E1`.
  - **Mood mapping:** [R3] "Song Structure Tag" — mood 1 (High) = Intro/Up/
    Down/Chorus/Outro with subtypes 1-3; mood 2 (Mid) = Intro/Verse 1-6/
    Chorus/Bridge/Outro; mood 3 (Low) = Intro/Verse 1-2/Chorus/Bridge/Outro.
- [ ] If present, parse and expose via appropriate API. This is used for
  lighting control on CDJ-3000. Low priority for Mixxx export use case.

### 4.2 Extended Cue Data (PCO2)

- [x] PCO2 binary format is well-documented. See §2.1 PCO2 entry above.
  - **Binary:** [R2][R3][R8] — variable-length entries with hot_cue number,
    type, time_ms, loop_time_ms, color_id, loop_numerator/denominator,
    UTF-16BE comment, color_code + RGB color.
- [ ] When writing, produce PCO2 tags with labels and colours when the data
  is available. Fall back to PCOB for basic cue data.
- [ ] When reading, prefer PCO2 over PCOB when both are present in `.EXT`.

### 4.3 3-Band Waveform Fidelity

- [ ] PWV6/PWV7 currently use synthetic sawtooth filler in our writer [R14].
  Need to accept real 3-band waveform data. Mixxx generates 3-band waveform
  data [R11 lines 297-313] with separate low/mid/high amplitudes per entry.
- [ ] PWVC tag — seen in `.2EX` files alongside PWV6/PWV7. [R7] has a
  `PWVCAnlzTag` class but no decoding logic is implemented. Research needed.

---

## 5. Database Features Not Yet Implemented (Tier 2–3)

### 5.1 Cue Table

The `cue` table schema exists and is created ([R1] §2.6.9), but:
- [ ] No `cue_table` C++ class exists for reading/writing. See [R9] `Cue`
  model for the complete column list.
- [ ] The SPEC ([R1] §2.6.9) notes that real exports leave `cue` empty (cues
  are in ANLZ). All 5 verified export samples have empty `cue` tables.
  However, populating it may improve compatibility with desktop rekordbox
  when it re-imports an export.
- [ ] Decision needed: populate `cue` rows on track creation, or leave empty
  per observed exports? **Recommendation:** leave empty to match observed
  exports. ANLZ is authoritative for cues.

### 5.2 History Table

- [ ] No `history_table` or `history_content_table` C++ classes. See [R9] for
  the SQLAlchemy models.
- [ ] History is a player-generated feature (recording what was played).
  Export use case is low priority but may matter for round-trip workflows.

### 5.3 My Tag Table

- [ ] No `myTag_table` or `myTag_content_table` C++ classes. See [R9] for
  models.
- [ ] rekordbox seeds a default taxonomy (Genre/Components/Situation). Our
  `insert_default_catalogues()` does not seed `myTag` rows — matches djay Pro
  which also emits empty `myTag` ([R1] §2.6.16).
- [ ] Low priority for export-only use case.

### 5.4 Hot Cue Bank List Table

- [ ] No `hotCueBankList_table` or `hotCueBankList_cue_table` C++ classes.
  See [R9] for models.
- [ ] Low priority for export-only use case.

### 5.5 Recommended Like Table

- [ ] No `recommendedLike_table` C++ class. See [R9] for model.
- [ ] Very low priority.

---

## 6. Schema Versioning (Tier 2)

- [ ] Define a `onelibrary_schema` enum (parallel to
  `djinterop::engine::engine_schema` at
  `include/djinterop/engine/engine_schema.hpp`). Currently hardcoded
  `"OneLibrary 1000"` in `database_impl::version_name()` [R18].
- [ ] Use the schema version for feature detection (`supports_feature` already
  works; versioning allows future-proofing).
- [ ] The SPEC ([R1] §2.2) documents `dbVersion = "1000"` in the `property`
  table. Future versions may appear. Prepare for `"1001"`, etc.

---

## 7. Edge Cases & Robustness (Tier 2)

### 7.1 Ordering

- [ ] `database_impl::create_root_playlist_after()` — `sequenceNo` reshuffling
  not implemented. Currently ignores `after` parameter [R18
  `database_impl.cpp`].
- [ ] `playlist_impl::create_sub_playlist_after()` — same issue [R18
  `playlist_impl.cpp`].
- [ ] `playlist_impl::add_track_after()` — ignores `after`, appends to end
  [R18 `playlist_impl.cpp`].
- **Reference for correct ordering:** SPEC ([R1] §2.4) — tree `sequenceNo`
  is 0-based for rekordbox, 1-based for djay Pro. Membership `sequenceNo` is
  1-based. Siblings ordered by `sequenceNo` ascending.

### 7.2 sequenceNo Base Convention

- [ ] SPEC ([R1] §2.4) notes rekordbox uses 0-based, djay Pro uses 1-based
  for tree `sequenceNo`. The current code auto-assigns but does not explicitly
  choose a convention. Should follow rekordbox (0-based) for maximum
  compatibility.

### 7.3 WAL Management

- [ ] `onelibrary::create()` checkpoints the WAL after creation via
  `PRAGMA wal_checkpoint(TRUNCATE)`. `onelibrary::load()` does not checkpoint.
  Should we checkpoint on close/destroy to avoid stale WAL files?
- [ ] The SPEC ([R1] §2.1) says real exports ship with an active WAL. Our
  create path truncates it, which deviates from observed exports. Consider
  leaving WAL active to match real exports.

### 7.4 Date Format Consistency

- [ ] SPEC ([R1] §2.4) notes date columns can be `"YYYY-MM-DD"` (rekordbox)
  or `"YYYY-MM-DD HH:MM:SS"` (djay Pro). We currently emit the full timestamp
  format. See [R9] `DateTime` type decorator which handles both formats on
  read. This should be documented as intentional.

### 7.5 `NULL` vs `0` for Unset FKs

- [ ] SPEC ([R1] §2.4) requires treating both `NULL` and `0` as "unset". The
  current code uses `0` for unset references. The DDL ([R1] §2.6) doesn't
  declare `NOT NULL`, so `NULL` is also valid. Ensure reads handle both.

### 7.6 Track Deletion Cleanup

- [ ] `remove_track()` deletes the `content` row but does not:
  - Remove the track from playlists (`playlist_content`).
  - Delete orphaned reference table rows.
  - Delete ANLZ files.
  - Update `property.numberOfContents`.

### 7.7 Orphaned Reference Rows

- [ ] Artists, albums, genres, labels, and keys are never garbage-collected.
  Once created, they persist even if no tracks reference them. This is
  consistent with real exports (rekordbox does not GC them either) but should
  be documented.

---

## 8. Public API Gaps (Tier 2–3)

### 8.1 ISRC Field

- [ ] Add `std::optional<std::string> isrc` to `track_snapshot` at
  `include/djinterop/track_snapshot.hpp`.
- [ ] Add `isrc()` / `set_isrc()` to `track_impl` base [R20].
- [ ] Implement in all three backends (Engine v2, Engine v3, OneLibrary).
- [ ] Update `operator==` and `operator<<` for `track_snapshot`.

### 8.2 Image/Artwork in track_snapshot

- [ ] `track_snapshot` has no artwork/image field. OneLibrary supports
  per-track artwork via `content.image_id` ([R1] §2.6.8, [R9]). Engine Prime
  also supports artwork. Consider adding `std::optional<std::string>
  artwork_path` or similar to `track_snapshot`. Cross-cutting change.

### 8.3 album_artist in track_snapshot

- [ ] `track_snapshot` has `artist` but no `album_artist`. OneLibrary's
  `album` table carries an `artist_id` (album artist) ([R1] §2.6.3, [R9]).
  The low-level `track_info` struct has `album_artist`. Consider adding to
  `track_snapshot`.

---

## 9. Test Coverage (Tier 1–2)

- [ ] Unit tests for ANLZ reader: parse real `.DAT`/`.EXT`/`.2EX` files.
  Test data available in:
  - `tmp/pyrekordbox/.testdata/rekordbox 6/backup/share/PIONEER/USBANLZ/` —
    6 tracks with full ANLZ sets (.DAT, .EXT, .2EX).
  - `tmp/pyrekordbox/.testdata/rekordordbox 5/backup/PIONEER/USBANLZ/` — 6
    tracks with legacy ANLZ (.DAT, .EXT only).
  - `PIONEER_DJAY_ONE/USBANLZ/` and `PIONEER_DJAY_MULT/USBANLZ/` in our
    own testdata.
- [ ] Unit tests for ANLZ writer round-trip (write → read → compare).
- [ ] Unit tests for `track_impl` bridge (snapshot/update round-trip for all
  fields, including ANLZ-backed ones).
- [ ] Unit tests for `database_impl` bridge (create, load, tracks, playlists,
  feature flags).
- [ ] Unit tests for `playlist_impl` bridge (children, ordering, rename,
  reparent).
- [ ] Integration test: create a OneLibrary export, add tracks with cues/
  beatgrid/waveforms, read back, verify all fields survive round-trip.
- [ ] Test with real hardware: export from Mixxx → USB → load on XDJ-AZ /
  OPUS-QUAD / OMNIS-DUO.

---

## 10. Mixxx Integration (Consumer — Out of Scope for libdjinterop)

This section is informational for planning. The actual work lives in the Mixxx
repository.

### 10.1 What Mixxx Needs to Build

Following the existing `EnginePrimeExportJob` pattern ([R11][R12][R13]):

1. **`OneLibraryExportRequest`** — struct with volume root dir, playlist/
   crate IDs to export, schema version. Model after [R12].
2. **`OneLibraryExportJob`** — QThread subclass, model after [R13], that:
   - Calls `djinterop::onelibrary::create_or_load_database(dir, created)`.
   - Checks `pDb->supports_feature(feature::supports_nested_crates)` — will
     be `false` for OneLibrary; skip crate logic. Map crates to playlist
     folders instead.
   - Maps Mixxx tracks → `track_snapshot` → `pDb->create_track(snapshot)`.
     See [R11 lines 180-313] for the exact mapping code (title, artist,
     album, genre, bpm, duration, bitrate, rating, key, beatgrid, hot_cues,
     loops, waveform, main_cue).
   - Maps Mixxx playlists/crates → OneLibrary playlists (folders for crates).
   - Calls ANLZ writer for each track, or lets `track_impl::update()` handle
     it once ANLZ is wired into the bridge (§2.2).
   - Copies audio files to `Contents/` on the volume.
3. **UI in `dlglibraryexport.cpp`** — add "AlphaTheta / OneLibrary" as an
   export target alongside "Engine DJ (Denon)".

### 10.2 Key Differences from Engine Prime Export

| Concern | Engine Prime | OneLibrary |
|---------|-------------|------------|
| DB location | `Engine Library/m.db` | `.PIONEER/rekordbox/exportLibrary.db` ([R1] §2.1) |
| DB encryption | None | SQLCipher, static passphrase ([R1] §2.3.2) |
| Crates | Supported | Not supported (playlists only) |
| Cues/waveforms | In DB (`TrackData` blob) | In ANLZ sidecar files ([R1] §2.11) |
| Path conventions | Relative to `Engine Library/` | Volume-relative, starting with `/` ([R1] §2.1) |
| Schema version | `engine_schema` enum | String `"1000"` in `property.dbVersion` ([R1] §2.2) |
| Artwork | DB-embedded | External files under `.PIONEER/Artwork/` ([R1] §2.6.8) |
| Nested playlists | Yes | Yes (`supports_nested_playlists`) |
| Duplicate tracks in playlist | Version-dependent | Yes (`playlists_support_duplicate_tracks`) |

### 10.3 Build System

Mixxx links libdjinterop statically via `FetchContent`. The new `_impl` source
files need to be added to the library target in `CMakeLists.txt` (already done
in the unstaged diff). No Mixxx-side build changes needed as long as
libdjinterop's CMake exports all sources correctly.

---

## 11. Implementation Order (Suggested)

### Phase 1 — Core ANLZ Bridge (enables Mixxx integration)
1. ANLZ reader (parse .DAT/.EXT/.2EX) — use [R6][R7][R8] as reference
2. Wire ANLZ reader into `track_impl::snapshot()` [R18]
3. Accept real beatgrid/waveform/cue data in ANLZ writer [R14][R15]
4. Wire ANLZ writer into `track_impl::update()` [R18]
5. Implement `track_impl::key()` / `set_key()` (key_id ↔ musical_key)
6. All currently-no-op setters in `track_impl` (beatgrid, hot_cues, loops,
   waveform, main_cue, average_loudness, last_played_at)

### Phase 2 — Robustness & Polish
7. `sequenceNo` ordering (create_*_after, add_track_after)
8. Track deletion cleanup
9. Schema versioning (`onelibrary_schema` enum)
10. ISRC field in `track_snapshot` (cross-cutting)
11. Test coverage

### Phase 3 — Extended Features
12. Cue table populate-or-not decision
13. History, MyTag, HotCueBankList tables
14. Phrase analysis (PSSI tag)
15. Extended cue data (PCO2) — labels, colours, quantized loops
16. 3-band waveform from real data
17. album_artist in track_snapshot

---

## 12. Quick Reference Index

| Ref | Path |
|-----|------|
| [R1] | `SPEC.md` |
| [R2] | `tmp/crate-digger/src/main/kaitai/rekordbox_anlz.ksy` |
| [R3] | `tmp/crate-digger/doc/modules/ROOT/pages/anlz.adoc` |
| [R4] | `tmp/pyrekordbox/docs/source/formats/anlz.md` |
| [R5] | `tmp/pyrekordbox/docs/source/formats/devicelib_plus.md` |
| [R6] | `tmp/pyrekordbox/pyrekordbox/anlz/file.py` |
| [R7] | `tmp/pyrekordbox/pyrekordbox/anlz/tags.py` |
| [R8] | `tmp/pyrekordbox/pyrekordbox/anlz/structs.py` |
| [R9] | `tmp/pyrekordbox/pyrekordbox/devicelib_plus/models.py` |
| [R10] | `tmp/pyrekordbox/pyrekordbox/devicelib_plus/database.py` |
| [R11] | `tmp/mixxx/src/library/export/engineprimeexportjob.cpp` |
| [R12] | `tmp/mixxx/src/library/export/engineprimeexportrequest.h` |
| [R13] | `tmp/mixxx/src/library/export/engineprimeexportjob.h` |
| [R14] | `src/djinterop/onelibrary/anlz/anlz_writer.cpp` |
| [R15] | `src/djinterop/onelibrary/anlz/tag_writers.hpp` |
| [R16] | `src/djinterop/onelibrary/anlz/pmai_writer.hpp` |
| [R17] | `src/djinterop/onelibrary/anlz/hash.hpp` |
| [R18] | `src/djinterop/onelibrary/track_impl.cpp` |
| [R19] | `src/djinterop/engine/v3/track_impl.cpp` |
| [R20] | `src/djinterop/impl/track_impl.hpp` |
| [R21] | `include/djinterop/track_snapshot.hpp` |
| [R22] | `src/djinterop/onelibrary/schema.cpp` |
| [R23] | `example/onelibrary_export.cpp` |