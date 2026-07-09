# rekordbox USB Export Format Specification

**Format name:** rekordbox device export (USB drive / SD card)
**Libraries described:** Device Library (legacy, `export.pdb`) and Device Library
Plus, marketed as *OneLibrary* (`exportLibrary.db`)
**Device Library Plus version:** `property.dbVersion = "1000"`
**Specification revision:** 1.0

---

## Status of This Document

This document specifies the on-storage layout, container formats, encryption
scheme, relational schema, and external analysis files that make up a complete
rekordbox export written to a removable volume (USB drive or SD card) "for DJ
use". Such an export is what AlphaTheta / Pioneer DJ hardware reads to browse and
load tracks without re-analysing them.

A complete export contains **two parallel libraries** on the same volume:

- the **legacy Device Library** (`export.pdb` + `ANLZ*.DAT`/`ANLZ*.EXT`), read by
  older players; and
- **Device Library Plus / OneLibrary** (`exportLibrary.db` + `exportExt.pdb` +
  `ANLZ*.2EX`), read by newer players.

Both libraries, the shared external analysis (ANLZ) files, the cached artwork,
and the player settings files are all specified here.

> **Provenance notice.** These formats are proprietary and undocumented. This
> specification is a *community reconstruction* assembled from independent
> reverse-engineering efforts (see [References](#references)) and validated
> against real exports. It is **not** an official AlphaTheta / Pioneer DJ
> publication and carries no warranty. Claims confirmed against real exports are
> stated plainly; claims that remain inferred or unresolved are flagged with an
> *Implementation note* or a **TODO (research)** marker (tagged `[capture]` for
> items needing new export captures or `[decode]` for items resolvable from
> existing data). Verify all behaviour against real exports and hardware before
> relying on it.

### Requirement keywords

The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHOULD**, **SHOULD NOT**,
**MAY**, and **OPTIONAL** in this document are to be interpreted as described in
[RFC 2119](https://www.rfc-editor.org/rfc/rfc2119).

### How this document was validated

Where a statement is marked *verified*, it was checked against one or more real
exports decrypted and parsed directly. The sample set comprises five exports from
two independent producers (as of 2026-07):

- **rekordbox:** an empty export with both libraries (no tracks, no ANLZ); a
  2-track export with both libraries, ANLZ analysis, and settings files.
- **djay Pro:** an empty OneLibrary-only skeleton; a 1-track OneLibrary-only
  export with ANLZ and artwork; a 3-track export with 2 playlists, ANLZ, and
  artwork.

All five samples ship with an active SQLite write-ahead log. Observed
`dbVersion` is `"1000"` across all producers. [verified]

---

## How to Read This Specification

This specification is organised according to the
[Diátaxis](https://diataxis.fr/) framework. Choose the entry point that matches
your goal:

| If you want to…                                             | Read (Diátaxis mode) |
| ----------------------------------------------------------- | -------------------- |
| Understand *what* the format is and *why* it exists         | **[1. Explanation](#part-1--explanation)** (understanding-oriented) |
| Look up an exact file, table, column, unit, or constant     | **[2. Reference](#part-2--reference)** (information-oriented) |
| Accomplish a specific task (decrypt, read tracks)           | **[3. How-To Guides](#part-3--how-to-guides)** (task-oriented) |
| Follow a first end-to-end walkthrough                       | **[4. Tutorial](#part-4--tutorial)** (learning-oriented) |
| Determine whether an export or tool conforms                | **[5. Conformance](#part-5--conformance)** |

Reference material (Part 2) is normative. Explanation, How-To, and Tutorial
material is informative and provided to aid comprehension.

---

# Part 1 — Explanation

*Understanding-oriented. This part explains context and design. It contains no
normative requirements.*

## 1.1 What a rekordbox USB Export Is

A rekordbox USB export is a self-describing snapshot of a music collection
placed on a removable volume: the audio files themselves, plus databases and
analysis files describing their metadata, cue points, loops, beat grids,
waveforms, playlists, and history. DJ hardware reads these files to browse and
play the collection without re-analysing the audio.

The export is organised under a single top-level directory (`.PIONEER/`) on the
volume, alongside the audio files (conventionally under `Contents/`).

## 1.2 The Two Libraries

Historically the export used the **Device Library**: a page-structured
**DeviceSQL** binary named `export.pdb`, designed for constrained embedded
players. It is compact but awkward to extend, because adding fields or tables
requires manual management of page headers, row groups, and sequence numbers.

**Device Library Plus** (marketed as *OneLibrary*) is its successor. Instead of a
bespoke binary format it uses a standard **SQLite** database, encrypted with
**SQLCipher**, named `exportLibrary.db`. It was introduced with rekordbox 6.8 and
expanded in rekordbox 7.x. It adopts SQLite to gain a well-understood relational
model, forward extensibility (new columns and tables without a binary page
allocator), and the identifiers required to associate tracks with modern 3-band
frequency waveforms.

Device Library Plus does **not** replace the legacy Device Library. A complete
export contains **both**, side by side on the same medium, so that both older and
newer players can read the same collection:

- **Legacy** (`export.pdb` + `ANLZ*.DAT` + `ANLZ*.EXT`) — read by older players
  (e.g. CDJ-3000, XDJ-XZ).
- **Device Library Plus** (`exportLibrary.db` + `exportExt.pdb` + `ANLZ*.2EX`) —
  read by newer players (e.g. CDJ-3000X, XDJ-AZ, OPUS-QUAD, OMNIS-DUO).

The two libraries share one set of external analysis directories: the
`.DAT`/`.EXT`/`.2EX` files for a given track live in the *same* hash-derived
folder (see [§2.11](#211-external-analysis-files-anlz)).

The two libraries are **maintained independently on the device**. Edits made on
hardware that writes only one library (for example, a new History list created
while DJing) are recorded only in that library. Reconciling the two requires
re-importing to rekordbox on a computer and re-exporting.

> **Implementation note:** a **OneLibrary-only** export (Device Library Plus +
> ANLZ + artwork, without the legacy `export.pdb`/`exportExt.pdb`) is a valid,
> shipping configuration. djay Pro produces OneLibrary-only exports that work on
> hardware. Whether newer players (CDJ-3000X et al.) require the legacy
> `export.pdb` merely to mount the volume, or can operate from Device Library
> Plus + ANLZ alone, is unconfirmed and only checkable on hardware. A complete
> rekordbox export always ships both.

## 1.3 Relationship to the rekordbox Master Database

The desktop rekordbox library lives in `master.db`, itself a SQLCipher-encrypted
SQLite database. Device Library Plus is best understood as a **slimmed,
export-oriented projection** of `master.db`:

- It contains a **subset** of the master schema — only what a player needs to
  browse and play.
- It **drops cloud-synchronisation bookkeeping** (`usn`, `rb_local_synced`,
  `rb_data_status`, per-row `UUID`, `created_at`/`updated_at`, etc.).
- It **replaces VARCHAR UUID primary keys with plain sequential integers**,
  which are cheaper for embedded players to index.
- It uses **volume-relative paths** rather than absolute local paths.
- It uses a **different encryption key** from `master.db`.

A full comparison appears in [Appendix A](#appendix-a--relationship-to-masterdb).
The two databases are otherwise conceptually aligned; `content.masterDbId` and
`content.masterContentId` retain the linkage back to the originating `master.db`
rows.

## 1.4 Design Principles (Informative)

1. **The databases are authoritative for metadata; ANLZ files are authoritative
   for signal analysis and performance data.** The databases store *where* the
   analysis lives and lightweight references, but the detailed beat grids,
   waveforms, cues, and loops live in external ANLZ files
   (`.DAT`/`.EXT`/`.2EX`). In observed exports the Device Library Plus `cue`
   table is empty and cues are carried entirely in ANLZ (see
   [§2.6.9](#269-cue) and [§2.11](#211-external-analysis-files-anlz)).
2. **Integer identifiers are export-local.** They are stable *within* one export
   but are not global identifiers; use `masterContentId` to correlate across
   exports of the same collection.
3. **Trees are modelled by self-reference.** Playlists, History, custom tags,
   and hot-cue banks all use the same folder/leaf pattern with a `*_id_parent`
   column.
4. **Analysis file locations are computed, not stored.** Players locate a track's
   ANLZ directory by hashing the audio file path, ignoring any stored path (see
   [§2.11.1](#2111-usbanlz-directory-hash)).

---

# Part 2 — Reference

*Information-oriented and normative. This part defines the format precisely.*

## 2.1 On-Storage Layout

All export data resides under the top-level `.PIONEER/` directory of the volume,
alongside the audio files. The directory name is dot-prefixed as written to the
export volume, and the dot-prefixed form is embedded in database paths (e.g.
`/.PIONEER/Artwork/...`). *Implementation note:* rekordbox's own local working
directory is named `PIONEER` without a dot; the dot-prefixed `.PIONEER` is the
form used on exported volumes.

```
<volume root>/
├── Contents/                         Audio files (referenced by content.path, ANLZ PPTH, PDB rows)
├── .PIONEER/
│   ├── rekordbox/
│   │   ├── exportLibrary.db          Device Library Plus — SQLCipher-encrypted SQLite database
│   │   ├── exportLibrary.db-wal      SQLite write-ahead log (present when WAL active)
│   │   ├── exportLibrary.db-shm      SQLite shared-memory index (transient)
│   │   ├── exportExt.pdb             Extended DeviceSQL database (OneLibrary-era)
│   │   ├── export.pdb                Legacy Device Library (DeviceSQL binary)
│   │   └── export.pdb.bak            Legacy backup (player-generated on device)
│   ├── USBANLZ/
│   │   └── P<XXX>/<HHHHHHHH>/         Hash-derived per-track directory (see §2.11.1)
│   │       ├── ANLZ0000.DAT          Legacy analysis (beat grid, mono waveform, cues)
│   │       ├── ANLZ0000.EXT          Extended analysis (colour waveforms, ext. cues/beatgrid, phrase)
│   │       └── ANLZ0000.2EX          Device Library Plus 3-band frequency waveforms
│   ├── Artwork/
│   │   └── NNNNN/...                 Cached artwork referenced by image.path
│   ├── MYSETTING.DAT                 Player "My Settings" (rekordbox-only, see §2.12)
│   ├── MYSETTING2.DAT                Player "My Settings" page 2 (rekordbox-only)
│   ├── DJMMYSETTING.DAT              DJM mixer settings (rekordbox-only)
│   └── DEVSETTING.DAT                Device settings (rekordbox-only)
└── (audio files)
```

- Paths stored inside the databases and ANLZ files (`content.path`,
  `image.path`, `content.analysisDataFilePath`, ANLZ `PPTH`) are
  **volume-relative**, use forward slashes, begin with `/`, and are UTF-8 (e.g.
  `/Contents/Justice/✝/01 Genesis.flac`). They **MUST** be resolved relative to
  the volume root, not the `.PIONEER/` directory. [verified]
- Artwork and analysis paths embed the `.PIONEER/` prefix (e.g.
  `/.PIONEER/Artwork/00001/b1.jpg`,
  `/.PIONEER/USBANLZ/P031/0002FB19/ANLZ0000.DAT`). [verified]
- A reader that consumes Device Library Plus **MUST** locate the database at
  `.PIONEER/rekordbox/exportLibrary.db` and **MUST** honour a present `-wal` file
  (open the database through a SQLite/SQLCipher engine that applies the WAL),
  rather than reading the main file in isolation, or it risks observing stale
  data. [verified: real exports from both rekordbox and djay Pro have shipped an
  active `-wal`]
- The player settings files (`MYSETTING*.DAT`, `DJMMYSETTING.DAT`,
  `DEVSETTING.DAT`) are **rekordbox-only**. Third-party producers (e.g. djay
  Pro) omit them and produce working exports. A Conforming Export **MAY** omit
  these files. [verified]
- Similarly, `export.pdb` and `exportExt.pdb` are rekordbox-only. A
  **OneLibrary-only** export (Device Library Plus + ANLZ + artwork, without the
  legacy databases) is a valid configuration produced by at least one shipping
  third-party producer. [verified]

## 2.2 Device Library Plus Container Format

The container is a standard **SQLite 3** database file. Because it is encrypted
in full (see [§2.3](#23-encryption)), the file does **not** begin with the
plaintext `"SQLite format 3\000"` magic; the first bytes are the SQLCipher salt
followed by ciphertext. A reader **MUST** treat the file as opaque until it has
been unlocked through SQLCipher.

Observed database-level settings: `page_size = 4096`, `user_version = 0`.
[verified]

## 2.3 Encryption

### 2.3.1 Cipher parameters

The database is encrypted with **SQLCipher 4**, using the SQLCipher 4 default
parameter set. A reader **MUST** open the database using these parameters (they
are the SQLCipher 4 defaults, so no custom `cipher_*` PRAGMAs are required beyond
selecting SQLCipher 4 / "legacy 4" compatibility):

| Parameter                | Value                    |
| ------------------------ | ------------------------ |
| Cipher                   | AES-256 in CBC mode      |
| Page size                | 4096 bytes               |
| KDF                      | PBKDF2-HMAC-SHA512       |
| KDF iterations           | 256000                   |
| HMAC                     | HMAC-SHA512              |
| Plaintext header size    | 0 (whole file encrypted) |
| Salt                     | First 16 bytes of file   |

The value supplied to `PRAGMA key` is treated by SQLCipher as a **text
passphrase** (not a raw hex key), so SQLCipher derives the encryption key from it
via PBKDF2 using the per-file salt. Decryption of a real export succeeds with
these parameters and `PRAGMA integrity_check` returns `ok`. [verified]

### 2.3.2 Key provisioning

Every Device Library Plus database is encrypted with the **same static
passphrase**. The passphrase is not licence-bound or device-bound and has been
stable across all releases that produce this format.

The passphrase is stored inside rekordbox as an obfuscated constant. It is
recovered by base85-decoding an embedded blob, XOR-ing it with a fixed repeating
key, and inflating the result with zlib:

```python
import base64, zlib

# XOR key (16 bytes, applied cyclically)
BLOB_KEY = b"657f48f84c437cc1"

# Obfuscated passphrase blob (base85, RFC 1924 alphabet as used by base64.b85)
BLOB = b"PN_1dH8$oLJY)16j_RvM6qphWw`476>;C1cWmI#se(PG`j}~xAjlufj?`#0i{;=glh(SkW)y0>n?YEiD`l%t("

def deobfuscate(blob: bytes) -> str:
    data = base64.b85decode(blob)
    xored = bytes(b ^ BLOB_KEY[i % len(BLOB_KEY)] for i, b in enumerate(data))
    return zlib.decompress(xored).decode("utf-8")

passphrase = deobfuscate(BLOB)
# -> "r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls"
```

- The recovered passphrase is the 64-character string
  `r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls` and begins
  with the ASCII sequence `r8gd`, which serves as a cheap validity check.
  [verified]
- This passphrase is distinct from the `master.db` key
  (`402fd482c38817c35ffa8ffb8c7d93143b749e7d315df7a81732a1ff43608497`), which is
  a different value and is supplied as a raw key rather than a passphrase.

### 2.3.3 Opening the database

Conceptually, opening the database is:

```sql
PRAGMA cipher = 'sqlcipher';
PRAGMA legacy = 4;                 -- SQLCipher 4 compatibility
PRAGMA key = 'r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls';
-- verify:
SELECT count(*) FROM sqlite_master;
```

If the `SELECT` succeeds, decryption is correct. See
[§3.1](#31-open-and-decrypt-the-database) for concrete language bindings.

## 2.4 Data Conventions

These conventions apply across the Device Library Plus tables and are normative.

| Concern            | Convention |
| ------------------ | ---------- |
| **Identifiers**    | Primary keys are `INTEGER`, unique within a single export. They are export-local, not global. Producers assign them sequentially and continue the sequence when appending. |
| **Foreign keys**   | Columns named `<entity>_id` reference `<entity>.<entity>_id`. |
| **Unset values**   | An unset reference is stored as `NULL` **or** `0`; both occur, sometimes within the same row. Readers **MUST** treat both `NULL` and `0` as "unset". [verified] |
| **Booleans**       | Stored as `INTEGER`: `0` = false, `1` = true. Columns are conventionally prefixed `is…` / `has…`. |
| **Tempo (BPM)**    | `content.bpmx100` is BPM × 100. Example: `12990` = 129.90 BPM. [verified] |
| **Track length**   | `content.length` is the track duration in **integer seconds**. Example: `234` = 3:54. [verified] |
| **Cue positions (DB)** | In the `cue` table, `inUsec`/`outUsec` are **microseconds** from the start of the track; `in150FramePerSec`/`out150FramePerSec` count frames of 1/150 s (≈ 6.667 ms). (Cue times in ANLZ are in milliseconds; see [§2.11](#211-external-analysis-files-anlz).) |
| **Ratings**        | `content.rating` is `0`–`5`. |
| **Dates**          | Date/time text is either date-only `YYYY-MM-DD` or a full `YYYY-MM-DD HH:MM:SS` timestamp, depending on producer and column. Stored as text. [verified] |
| **Search strings** | `*ForSearch` columns hold a normalised, case-/accent-folded form used for on-device search. They are frequently `NULL` in real exports. Readers **MAY** ignore them; producers **MAY** leave them empty. [verified] |
| **Text encoding**  | All text is UTF-8. [verified] |
| **Sequence (trees)** | In tree tables (`playlist`, `history`, `myTag`, `hotCueBankList`), `sequenceNo` orders siblings ascending. rekordbox uses **0-based** (first sibling `= 0`); djay Pro uses **1-based** (first sibling `= 1`). Readers **MUST** treat `sequenceNo` as a relative order within siblings rather than assuming a base value. [verified] |
| **Sequence (membership)** | In membership tables (`playlist_content`, `history_content`, `hotCueBankList_cue`), `sequenceNo` orders members ascending and is **1-based** (first member `= 1`). [verified] |

*Implementation note:* the primary key value is unrelated to display order; always
order by `sequenceNo`. A track may belong to multiple playlists.

*Implementation note:* `content.analysedBits = 41` and `content.contentLink =
788224` (`0x0C0700`) are **fixed magic constants**, byte-identical across
rekordbox and djay Pro for every track in every observed export. A producer
**SHOULD** use these values verbatim. [verified]

*Implementation note:* `*ForSearch` columns are frequently `NULL` in real
exports. rekordbox leaves them all `NULL`; djay Pro sometimes sets
`artist.nameForSearch` equal to `name`. A producer **MAY** leave them `NULL`.

*Implementation note:* date columns (`dateAdded`, `dateCreated`, `createdDate`)
may be `"YYYY-MM-DD"` (rekordbox) or `"YYYY-MM-DD HH:MM:SS"` (djay Pro).
Readers **MUST** handle both formats. [verified]

> **TODO (research):** confirm the `sequenceNo` base convention for deeply
> nested folders; observed data is limited to root-level playlists and a
> small number of siblings. Folder nesting (`attribute = 1` with a non-zero
> `*_id_parent`) has not been observed in real data.

## 2.5 Schema Overview

Device Library Plus defines the following 22 tables. Types are SQLite storage
classes. The decrypted `CREATE TABLE` statements of real exports match this
table set, column names, and column order exactly. [verified]

| Table                | Role |
| -------------------- | ---- |
| `content`            | Tracks (the central table) |
| `artist`             | Artist / remixer / composer / lyricist names |
| `album`              | Albums |
| `genre`              | Genres |
| `label`              | Record labels |
| `key`                | Musical keys |
| `color`              | Colour palette for track colouring |
| `image`              | Artwork file references |
| `cue`                | Cue points, loops, hot cues, hot loops (schema; empty in observed exports — see §2.6.9) |
| `playlist`           | Playlist tree (folders + playlists) |
| `playlist_content`   | Track membership of playlists |
| `history`            | History session tree |
| `history_content`    | Track membership of history sessions |
| `hotCueBankList`     | Hot-cue bank list tree |
| `hotCueBankList_cue` | Cue membership of hot-cue bank lists |
| `myTag`              | Custom tag ("My Tag") tree |
| `myTag_content`      | Track membership of custom tags |
| `menuItem`           | Browse-menu item catalogue |
| `category`           | Which browse categories are shown, and order |
| `sort`               | Which sort options are shown, and order |
| `property`           | Device / database metadata (single row) |
| `recommendedLike`    | Track-to-track "related" relationships |

### 2.5.1 Entity relationships (informative)

```
                 ┌──────────┐
   artist ◄──────┤          ├──────► album ──► artist
   genre  ◄──────┤ content  ├──────► label
   key    ◄──────┤ (track)  ├──────► color
   image  ◄──────┤          ├──────► image
                 └────┬─────┘
                      │ content_id
        ┌─────────────┼───────────────┬───────────────┐
        ▼             ▼               ▼               ▼
      cue      playlist_content  history_content  myTag_content
        │             ▲               ▲               ▲
        ▼             │               │               │
 hotCueBankList_cue playlist        history          myTag
                    (tree)         (tree)           (tree)
```

## 2.6 Table Reference

Column tables list the SQLite type and notes. "PK" = logical primary key,
"FK → t" = logical foreign key into table `t`. These relationships are logical:
they are **not** declared as SQL `PRIMARY KEY`/`FOREIGN KEY` constraints in the
schema (see [§2.8](#28-indexes-and-referential-integrity)).

### 2.6.1 `content`

The central track table.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `content_id` | INTEGER | **PK.** Track identifier (referenced as `content_id` elsewhere). |
| `title` | TEXT | Track title. |
| `titleForSearch` | TEXT | Normalised title for search (often `NULL`). |
| `subtitle` | TEXT | Subtitle / mix name. |
| `bpmx100` | INTEGER | BPM × 100. |
| `length` | INTEGER | Duration in integer seconds. |
| `trackNo` | INTEGER | Track number within album. |
| `discNo` | INTEGER | Disc number within album. |
| `artist_id_artist` | INTEGER | FK → `artist`. Primary performing artist. |
| `artist_id_remixer` | INTEGER | FK → `artist`. Remixer. |
| `artist_id_originalArtist` | INTEGER | FK → `artist`. Original artist. |
| `artist_id_composer` | INTEGER | FK → `artist`. Composer. |
| `artist_id_lyricist` | INTEGER | FK → `artist`. Lyricist. |
| `album_id` | INTEGER | FK → `album`. |
| `genre_id` | INTEGER | FK → `genre`. |
| `label_id` | INTEGER | FK → `label`. |
| `key_id` | INTEGER | FK → `key`. Musical key. |
| `color_id` | INTEGER | FK → `color`. |
| `image_id` | INTEGER | FK → `image`. Artwork. |
| `djComment` | TEXT | DJ comment / notes. |
| `rating` | INTEGER | `0`–`5`. |
| `releaseYear` | INTEGER | Year of release. |
| `releaseDate` | TEXT | Release date. |
| `dateCreated` | TEXT | File creation date (may be `NULL`). |
| `dateAdded` | TEXT | Date added to library. |
| `path` | TEXT | Volume-relative audio file path. |
| `fileName` | TEXT | File name. |
| `fileSize` | INTEGER | File size in bytes. |
| `fileType` | INTEGER | Audio format code; see [§2.7.1](#271-filetype-content). |
| `bitrate` | INTEGER | Bit rate in kbps (`0` for lossless such as FLAC). |
| `bitDepth` | INTEGER | Bit depth in bits (may be `0` if not populated). |
| `samplingRate` | INTEGER | Sample rate in Hz. |
| `isrc` | TEXT | ISRC code. |
| `djPlayCount` | INTEGER | Play count. |
| `isHotCueAutoLoadOn` | INTEGER | Boolean: auto-load hot cues for this track. |
| `isKuvoDeliverStatusOn` | INTEGER | Boolean: KUVO public delivery flag. |
| `kuvoDeliveryComment` | TEXT | KUVO delivery comment. |
| `masterDbId` | INTEGER | Originating `master.db` identifier. `0` for producers other than rekordbox. |
| `masterContentId` | INTEGER | Originating `master.db` content identifier (cross-export correlation). `0` for producers other than rekordbox. |
| `analysisDataFilePath` | TEXT | Volume-relative path to the ANLZ `.DAT` file (see [§2.11](#211-external-analysis-files-anlz)). |
| `analysedBits` | INTEGER | Analysis flags/level. Fixed magic constant `41` across all producers and all tracks. [verified] |
| `contentLink` | INTEGER | Cross-reference identifier. Fixed magic constant `788224` (`0x0C0700`) across all producers and all tracks. [verified] |
| `hasModified` | INTEGER | Boolean: modified since export. |
| `cueUpdateCount` | INTEGER | Monotonic counter of cue edits (may be `NULL`). |
| `analysisDataUpdateCount` | INTEGER | Monotonic counter of analysis updates (may be `NULL`). |
| `informationUpdateCount` | INTEGER | Monotonic counter of metadata updates (may be `NULL`). |

*Implementation note:* the presence of the `djPlayCount` column (and the
`album.image_id` column, and the capital-`O` `cue.OutFileOffsetInBlock` column)
matches the real schema even though some public documentation omits or
misnames them. [verified]

### 2.6.2 `artist`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `artist_id` | INTEGER | **PK.** |
| `name` | TEXT | Artist name. |
| `nameForSearch` | TEXT | Normalised name for search (often `NULL`). |

### 2.6.3 `album`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `album_id` | INTEGER | **PK.** |
| `name` | TEXT | Album name. |
| `artist_id` | INTEGER | FK → `artist`. Album artist. |
| `image_id` | INTEGER | FK → `image`. Album artwork. |
| `isComplation` | INTEGER | Boolean: compilation. *(Column name carries the original misspelling of "compilation".)* |
| `nameForSearch` | TEXT | Normalised name for search (often `NULL`). |

### 2.6.4 `genre`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `genre_id` | INTEGER | **PK.** |
| `name` | TEXT | Genre name. |

### 2.6.5 `label`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `label_id` | INTEGER | **PK.** |
| `name` | TEXT | Label name. |

### 2.6.6 `key`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `key_id` | INTEGER | **PK.** |
| `name` | TEXT | Musical key name (e.g. classical or Camelot notation as authored). |

### 2.6.7 `color`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `color_id` | INTEGER | **PK.** |
| `name` | TEXT | Colour name. |

The colour palette has 8 fixed entries with `color_id` 1–8: Pink, Red, Orange,
Yellow, Green, Aqua, Blue, Purple. This palette is present even in an export with
zero tracks. [verified]

### 2.6.8 `image`

| Column | Type | Notes |
| ------ | ---- | ----- |
| `image_id` | INTEGER | **PK.** |
| `path` | TEXT | Volume-relative path to the artwork file (e.g. `/.PIONEER/Artwork/00001/b1.jpg`). |

### 2.6.9 `cue`

Schema for memory cues, hot cues, loops, and hot loops for a track, one row per
cue.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `cue_id` | INTEGER | **PK.** |
| `content_id` | INTEGER | FK → `content`. |
| `kind` | INTEGER | Cue type; see [§2.7.2](#272-kind-cue). |
| `colorTableIndex` | INTEGER | Index into the on-device cue colour palette (`-1` = none). |
| `cueComment` | TEXT | Cue label / comment. |
| `isActiveLoop` | INTEGER | Boolean: loop is active on reaching the point. |
| `beatLoopNumerator` | INTEGER | Beat-loop length numerator. |
| `beatLoopDenominator` | INTEGER | Beat-loop length denominator. |
| `inUsec` | INTEGER | In-point in microseconds. |
| `outUsec` | INTEGER | Out-point in microseconds (loops only; `≤ 0`/unset for a plain cue). |
| `in150FramePerSec` | INTEGER | In-point in 1/150 s frames. |
| `out150FramePerSec` | INTEGER | Out-point in 1/150 s frames. |
| `inMpegFrameNumber` | INTEGER | MPEG frame number for VBR/ABR seeking (in). |
| `outMpegFrameNumber` | INTEGER | MPEG frame number for VBR/ABR seeking (out). |
| `inMpegAbs` | INTEGER | Absolute byte offset of the in MPEG frame. |
| `outMpegAbs` | INTEGER | Absolute byte offset of the out MPEG frame. |
| `inDecodingStartFramePosition` | INTEGER | Decoder start frame (in). |
| `outDecodingStartFramePosition` | INTEGER | Decoder start frame (out). |
| `inFileOffsetInBlock` | INTEGER | In-point byte offset within decode block. |
| `OutFileOffsetInBlock` | INTEGER | Out-point byte offset within decode block. *(Note the leading capital `O` in this column name.)* |
| `inNumberOfSampleInBlock` | INTEGER | Sample count within block (in). |
| `outNumberOfSampleInBlock` | INTEGER | Sample count within block (out). |

**Cue storage in practice.** In every observed export — across 5 samples from 2
independent producers (rekordbox and djay Pro), including exports made after
deliberately adding cues — the `cue`, `hotCueBankList`, `hotCueBankList_cue`,
`history`, `history_content`, `myTag_content`, and `recommendedLike` tables are
**empty**. The authoritative store for cues, loops, hot cues, and hot loops is
the external ANLZ files (`PCOB` / `PCO2` tags), not these tables. A reader that
needs cues **MUST** read them from ANLZ (see [§2.11](#211-external-analysis-files-anlz));
the DB `cue` table cannot be relied upon to contain them. [verified]

> **TODO (research):** whether any player ever reads cues from the DB `cue`
> table (rather than only from ANLZ) is unconfirmed. The schema is retained
> because the table exists in every export.

If a row *is* present, interpret it as (informative): `outUsec > 0` ⇒ loop
(region), otherwise a point; `kind ≥ 1` ⇒ hot cue/loop, otherwise memory
cue/loop.

### 2.6.10 `playlist`

Self-referential tree of playlist folders and playlists.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `playlist_id` | INTEGER | **PK.** |
| `sequenceNo` | INTEGER | Order among siblings (0-based). |
| `name` | TEXT | Playlist / folder name. |
| `image_id` | INTEGER | FK → `image`. Optional playlist artwork. |
| `attribute` | INTEGER | `0` = playlist, `1` = folder; see [§2.7.3](#273-attribute-tree-tables). |
| `playlist_id_parent` | INTEGER | FK → `playlist`. `0` at root. |

> **TODO (research):** folder nesting (`attribute = 1` with a non-zero
> `*_id_parent`) has not been observed in real data; only root-level leaves have
> been verified.

### 2.6.11 `playlist_content`

Membership of tracks in playlists.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `playlist_id` | INTEGER | FK → `playlist`. Part of logical PK. |
| `content_id` | INTEGER | FK → `content`. Part of logical PK. |
| `sequenceNo` | INTEGER | Track order within the playlist (1-based). |

### 2.6.12 `history`

Self-referential tree of History sessions (each session is a recorded set).

| Column | Type | Notes |
| ------ | ---- | ----- |
| `history_id` | INTEGER | **PK.** |
| `sequenceNo` | INTEGER | Order among siblings (0-based). |
| `name` | TEXT | Session / folder name. |
| `attribute` | INTEGER | Folder vs. session; see [§2.7.3](#273-attribute-tree-tables). |
| `history_id_parent` | INTEGER | FK → `history`. `0` at root. |

### 2.6.13 `history_content`

Membership of tracks in History sessions.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `history_id` | INTEGER | FK → `history`. Part of logical PK. |
| `content_id` | INTEGER | FK → `content`. Part of logical PK. |
| `sequenceNo` | INTEGER | Track order within the session (1-based). |

### 2.6.14 `hotCueBankList`

Self-referential tree of hot-cue bank lists.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `hotCueBankList_id` | INTEGER | **PK.** |
| `sequenceNo` | INTEGER | Order among siblings (0-based). |
| `name` | TEXT | Bank list / folder name. |
| `image_id` | INTEGER | FK → `image`. |
| `attribute` | INTEGER | Folder vs. leaf; see [§2.7.3](#273-attribute-tree-tables). |
| `hotCueBankList_id_parent` | INTEGER | FK → `hotCueBankList`. |

### 2.6.15 `hotCueBankList_cue`

Membership of cues in hot-cue bank lists.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `hotCueBankList_id` | INTEGER | FK → `hotCueBankList`. Part of logical PK. |
| `cue_id` | INTEGER | FK → `cue`. Part of logical PK. |
| `sequenceNo` | INTEGER | Order within the bank list (1-based). |

### 2.6.16 `myTag`

Self-referential tree of custom tags ("My Tag").

| Column | Type | Notes |
| ------ | ---- | ----- |
| `myTag_id` | INTEGER | **PK.** |
| `sequenceNo` | INTEGER | Order among siblings (0-based). |
| `name` | TEXT | Tag / category name. |
| `attribute` | INTEGER | Folder/category vs. tag; see [§2.7.3](#273-attribute-tree-tables). |
| `myTag_id_parent` | INTEGER | FK → `myTag`. `0` at root. |

rekordbox seeds a default My Tag taxonomy (Genre / Components / Situation and
their child tags). This default taxonomy is producer-specific: some producers
seed it and some emit an empty `myTag` table. [verified]

### 2.6.17 `myTag_content`

Assignment of custom tags to tracks.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `myTag_id` | INTEGER | FK → `myTag`. Part of logical PK. |
| `content_id` | INTEGER | FK → `content`. Part of logical PK. |

### 2.6.18 `menuItem`

Catalogue of browse-menu items available on the device.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `menuItem_id` | INTEGER | **PK.** |
| `kind` | INTEGER | Menu item kind; see [§2.7.4](#274-kind-menuitem). |
| `name` | TEXT | Display name. *Implementation note:* system/localisable labels are wrapped in `U+FFFA … U+FFFB` delimiters (e.g. `␚GENRE␛`). |

A default `menuItem` catalogue (27 rows observed) is present even in an export
with zero tracks. [verified]

### 2.6.19 `category`

Which browse categories are shown, and in what order.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `category_id` | INTEGER | **PK.** |
| `menuItem_id` | INTEGER | FK → `menuItem`. |
| `sequenceNo` | INTEGER | Display order. |
| `isVisible` | INTEGER | Boolean: shown in the browse menu. |

A default `category` set (21–22 rows observed, varying slightly by producer) is
present even in an export with zero tracks. [verified]

### 2.6.20 `sort`

Which sort options are shown, and in what order.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `sort_id` | INTEGER | **PK.** |
| `menuItem_id` | INTEGER | FK → `menuItem`. |
| `sequenceNo` | INTEGER | Display order. |
| `isVisible` | INTEGER | Boolean: available as a sort option. |
| `isSelectedAsSubColumn` | INTEGER | Boolean: shown as a secondary browse column. |

A default `sort` set (17 rows observed) is present even in an export with zero
tracks. [verified]

### 2.6.21 `property`

Single-row table describing the export.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `deviceName` | TEXT | **PK.** Device / export name. Frequently the volume label; may be empty (`''`). rekordbox writes `""`; djay Pro writes `"rbx"`. [verified] |
| `dbVersion` | TEXT | Database format version (`"1000"` for this revision). [verified] |
| `numberOfContents` | INTEGER | Intended to equal `SELECT count(*) FROM content`. **Unreliable.** djay Pro always writes `0` regardless of actual track count (confirmed across 0-, 1-, and 3-track exports). rekordbox writes the correct count. Readers **MUST** compute the true count from `content` rather than trusting this field. [verified] |
| `createdDate` | TEXT | Export creation date. Format varies: `"YYYY-MM-DD"` (rekordbox) or `"YYYY-MM-DD HH:MM:SS"` (djay Pro). Readers **MUST** handle both. [verified] |
| `backGroundColorType` | INTEGER | UI background colour selector (`0` observed in all samples). |
| `myTagMasterDBID` | INTEGER | `master.db` identifier for the custom-tag definitions. **Not a fixed constant.** djay Pro sets `0`; rekordbox sets a per-master-db value (e.g. `1853643511`). A non-rekordbox producer **MUST** set `0`. [verified] |

### 2.6.22 `recommendedLike`

"Related track" relationships with a strength rating.

| Column | Type | Notes |
| ------ | ---- | ----- |
| `content_id_1` | INTEGER | FK → `content`. Part of logical PK. |
| `content_id_2` | INTEGER | FK → `content`. Part of logical PK. |
| `rating` | INTEGER | Relationship strength (`0`–`5`). |
| `createdDate` | INTEGER | Creation timestamp. |

> **TODO (research):** `history`/`history_content`, `myTag_content`,
> `recommendedLike`, and `genre` have not been observed populated in any of the
> five sample exports. Their column semantics above are as designed but not
> data-verified. Populated `history` and `history_content` require a capture
> with a History session. [capture]

## 2.7 Enumerations

### 2.7.1 `fileType` (`content`)

| Value | Format |
| ----- | ------ |
| `0` / `1` | MP3 |
| `4` | M4A / AAC |
| `5` | FLAC (verified) |
| `11` | WAV |
| `12` | AIFF |

*Implementation note:* MP3 has been observed as both `0` and `1` depending on
encoder metadata. Readers **SHOULD** accept both.

### 2.7.2 `kind` (`cue`)

| Value | Meaning |
| ----- | ------- |
| `0` | Memory cue / memory loop |
| `1`–`8` | Hot cue / hot loop, where the value is the hot-cue button index (A = 1, B = 2, …, H = 8) |

*Implementation note:* the exact upper bound of hot-cue indices depends on the
device generation; treat `kind ≥ 1` as a hot cue and clamp to the device's
supported button count. (Note that the same memory/hot distinction — hot index
`0` for memory cues — is used in the ANLZ cue records; see
[§2.11.3](#2113-cues-and-loops).)

### 2.7.3 `attribute` (tree tables)

Applies to `playlist`, `history`, `hotCueBankList`, and `myTag`.

| Value | Meaning |
| ----- | ------- |
| `0` | Leaf (playlist / session / tag / bank list) |
| `1` | Folder / category container |

Additional attribute values may appear in `master.db`-derived data (e.g. smart
playlists); such values are not part of the base export and **SHOULD** be treated
as leaves unless explicitly supported.

### 2.7.4 `kind` (`menuItem`)

Browse-menu item kinds. This enumeration is device-dependent; the following
values have been observed. [verified]

| Value | Category |
| ----- | -------- |
| `128` | Genre |
| `129` | Artist |
| `130` | Album |
| `131` | Track |
| `132` | Playlist |
| `133` | BPM |
| `134` | Rating |
| `135` | Year |
| `136` | Remixer |
| `137` | Label |
| `138` | Original Artist |
| `139` | Key |
| `140` | Date Added |
| `141` | Cue |
| `142` | Color |
| `144` | Folder |
| `145` | Search |
| `146` | Time |
| `147` | Bitrate |
| `148` | File Name |
| `149` | History |
| `150` | Comments |
| `151` | DJ Play Count |
| `152` | Hot Cue Bank |
| `161` | Default |
| `162` | Alphabet |
| `170` | Matching |

## 2.8 Indexes and Referential Integrity

The Device Library Plus schema declares **no** SQL `PRIMARY KEY` or `FOREIGN KEY`
constraints on the join/link tables; the "PK"/"FK" designations in
[§2.6](#26-table-reference) are logical only. The schema declares exactly four
explicit indexes: [verified]

```
index_playlist_content_playlist_id            on playlist_content(playlist_id)
index_myTag_content_content_id                on myTag_content(content_id)
index_myTag_content_myTag_id                  on myTag_content(myTag_id)
index_hotCueBankList_cue_hotCueBankList_id    on hotCueBankList_cue(hotCueBankList_id)
```

Because the database engine does not enforce integrity, referential consistency
is a producer responsibility. A conforming producer **SHOULD** ensure:

1. Every non-null/non-zero FK column references an existing row in the target
   table.
2. Every membership/link row references existing rows in both parent tables.
3. Tree `*_id_parent` chains are acyclic and terminate at a root (`0`/`NULL`).
4. `sequenceNo` values are unique among siblings sharing a parent/list.

A reader **MUST** tolerate violations gracefully (e.g. treat a dangling FK as
"unset", compute counts directly) rather than aborting. *Implementation note:*
`property.numberOfContents` is not always maintained by producers; see
[§2.6.21](#2621-property).

*Implementation note:* DDL keyword casing varies by producer (e.g. `PRIMARY KEY`
vs `primary key`); this is cosmetic and structurally irrelevant. Producers may
also add extra, non-standard tables (e.g. private per-producer data); a reader
**MUST** tolerate tables beyond the 22 specified here. [verified]

## 2.9 Default Catalogues

Independent of track content, a valid export carries default catalogue rows that
the browse UI relies on. These are present even in exports with zero tracks.
[verified]

### 2.9.1 `color` (8 rows)

Identical across all observed producers:

| `color_id` | `name` |
| ---------- | ------ |
| 1 | Pink |
| 2 | Red |
| 3 | Orange |
| 4 | Yellow |
| 5 | Green |
| 6 | Aqua |
| 7 | Blue |
| 8 | Purple |

### 2.9.2 `menuItem` (27 rows)

Identical `(menuItem_id, kind, name)` tuples across all observed producers.
Display names wrap system/localisable labels in `U+FFFA … U+FFFB` delimiters
(e.g. `␚GENRE␛`).

| `menuItem_id` | `kind` | Name |
| ------------- | ------ | ---- |
| 1 | 128 | `␚GENRE␛` |
| 2 | 129 | `␚ARTIST␛` |
| 3 | 130 | `␚ALBUM␛` |
| 4 | 131 | `␚TRACK␛` |
| 5 | 133 | `␚BPM␛` |
| 6 | 134 | `␚RATING␛` |
| 7 | 135 | `␚YEAR␛` |
| 8 | 136 | `␚REMIXER␛` |
| 9 | 137 | `␚LABEL␛` |
| 10 | 138 | `␚ORIGINAL ARTIST␛` |
| 11 | 139 | `␚KEY␛` |
| 12 | 141 | `␚CUE␛` |
| 13 | 142 | `␚COLOR␛` |
| 14 | 146 | `␚TIME␛` |
| 15 | 147 | `␚BITRATE␛` |
| 16 | 148 | `␚FILE NAME␛` |
| 17 | 132 | `␚PLAYLIST␛` |
| 18 | 152 | `␚HOT CUE BANK␛` |
| 19 | 149 | `␚HISTORY␛` |
| 20 | 145 | `␚SEARCH␛` |
| 21 | 150 | `␚COMMENTS␛` |
| 22 | 140 | `␚DATE ADDED␛` |
| 23 | 151 | `␚DJ PLAY COUNT␛` |
| 24 | 144 | `␚FOLDER␛` |
| 25 | 161 | `␚DEFAULT␛` |
| 26 | 162 | `␚ALPHABET␛` |
| 27 | 170 | `␚MATCHING␛` |

### 2.9.3 `sort` (17 rows)

Identical `(menuItem_id, sequenceNo, isVisible, isSelectedAsSubColumn)` tuples
across all observed producers (numeric `sort_id` values differ but are not
meaningful). The following sorts are visible (`isVisible = 1`): DEFAULT (seq 1),
ALPHABET (seq 2), ARTIST (seq 3), ALBUM (seq 4), BPM (seq 5), RATING (seq 6),
KEY (seq 7). All remaining sorts have `isVisible = 0`. [verified]

### 2.9.4 `category` (21 or 22 rows — producer-dependent)

- **djay Pro** (21 rows): visible categories are ARTIST, ALBUM, TRACK, KEY,
  PLAYLIST, HISTORY, SEARCH, MATCHING, FOLDER.
- **rekordbox** (22 rows): same as djay Pro plus DATE ADDED at `sequenceNo = 10`.

The `category_id` values differ between producers and are not meaningful; only
the `(menuItem_id, sequenceNo, isVisible)` tuples define the browse-menu layout.
[verified]

### 2.9.5 `myTag` default taxonomy

rekordbox seeds a 28-row default My Tag taxonomy with four top-level categories
(Genre, Components, Situation, "Untitled Column") and 24 child tags. Tag IDs are
large, non-consecutive values in the `7×10⁸`–`4×10⁹` range — these are
`master.db` identifiers, not export-local. djay Pro emits an empty `myTag` table
(0 rows).

The default taxonomy is producer-specific. A non-rekordbox producer **MAY** emit
an empty `myTag` table. [verified]

## 2.10 Legacy Device Library (`export.pdb`)

`export.pdb` is Pioneer's **DeviceSQL** database: a page-based binary format,
little-endian, with 4096-byte pages. It carries the same conceptual entities as
Device Library Plus (tracks, artists, albums, labels, genres, keys, colours,
artwork, playlists, history) in a compact embedded layout.

The format is fully implemented with read/write support by **rekordcrate**
(Rust); the canonical Kaitai struct definition is in **crate-digger**'s
`rekordbox_pdb.ksy`. This section documents the format at the level needed to
implement a conforming producer. For byte-level row layouts of every table type,
consult rekordcrate's `src/pdb/mod.rs` and crate-digger's Kaitai definitions.

### 2.10.1 File header (page 0)

Verified against a real export: [verified]

```
offset  field              value (example export)
0x00    magic              0x00000000
0x04    page_size          4096
0x08    num_tables         20
0x0C    next_unused_page   52
0x10    unknown            1
0x14    sequence           14   (MUST exceed the max data-page sequence)
0x18    padding            0
0x1C    table_pointers[]   num_tables × 16 bytes
```

Each 16-byte table pointer is `table_type (u32)`, `empty_candidate (u32)`,
`first_page (u32)`, `last_page (u32)`.

### 2.10.2 Table types

The complete table-type enumeration (rekordcrate `PlainPageType`):

| Type | Name | Role | Status |
| ---- | ---- | ---- | ------ |
| 0 | Tracks | Audio track metadata (21 string columns) | Full read/write |
| 1 | Genres | Genre names | Full read/write |
| 2 | Artists | Artist names | Full read/write |
| 3 | Albums | Album metadata | Full read/write |
| 4 | Labels | Label names | Full read/write |
| 5 | Keys | Musical key names | Full read/write |
| 6 | Colors | Colour palette (8 entries) | Full read/write |
| 7 | PlaylistTree | Playlist folder/playlist tree | Full read/write |
| 8 | PlaylistEntries | Track membership in playlists | Full read/write |
| 9 | Unknown(9) | — empty index page, 0 rows | Reproduce as-is |
| 10 | Unknown(10) | — empty index page, 0 rows | Reproduce as-is |
| 11 | HistoryPlaylists | History session tree | Full read/write |
| 12 | HistoryEntries | Track membership in history | Full read/write |
| 13 | Artwork | Artwork file references | Full read/write |
| 14 | Unknown(14) | — empty index page, 0 rows | Reproduce as-is |
| 15 | Unknown(15) | — empty index page, 0 rows | Reproduce as-is |
| 16 | Columns | Column metadata for browse UI | Full read/write |
| 17 | Menu | Menu configuration (category, visibility, sort order) | Full read/write |
| 18 | Unknown(18) | **Has data** — 17 rows even in empty exports | Byte-copy from known-good DB |
| 19 | History | History metadata | Full read/write |
| 20 | HistoryEntries (alt) | Present in enum; absent without history data | Full read/write |
| 21 | History (alt) | Present in enum; absent without history data | Full read/write |

**Unknown types with data:** Type 18 carries 17 rows of unknown content even in
empty exports and **MUST** be reproduced byte-for-byte from a known-good empty
`export.pdb`. Types 9, 10, 14, and 15 are empty index pages (0 rows) and are
safe to reproduce as-is. rekordcrate round-trips unknown types as raw
`Row::Unknown` bytes, enabling a byte-copy strategy.

### 2.10.3 Page structure

DeviceSQL uses two page kinds per table:

- **Index pages:** contain a row-group index mapping row-group numbers to data
  page numbers. The first page of each table is its index page.
- **Data pages:** store the actual rows. Each data page has a header with
  `page_index`, `sequence`, and `num_rows`, followed by a row-offset table
  pointing to each row within the page.

Rows are grouped and addressed by a `(row_group, row_index)` pair. Row groups
are allocated sequentially within a table.

### 2.10.4 Row layouts (key tables)

**Tracks (type 0):** 21 string columns stored as DeviceSQL-encoded strings
(short ASCII, long ASCII, or long UTF-16LE depending on length). Column
positions (from rekordcrate):

| Index | Column | Encoding |
| ----- | ------ | -------- |
| 0 | File path | String |
| 1 | File name | String |
| 2 | Title | String |
| 3 | Artist | String |
| 4 | Album | String |
| 5 | Genre | String |
| 6 | Key | String |
| 7 | BPM | String |
| 8 | Track number | String |
| 9 | Duration | String |
| 10 | Comment | String |
| 11 | Remixer | String |
| 12 | Label | String |
| 13 | Bit rate | String |
| 14 | Analyze path | String |
| 15 | Date added | String |
| 16 | Original artist | String |
| 17 | Composer | String |
| 18 | Color | String |
| 19 | ISRC | String (special ISRC variant) |
| 20 | Sample rate / depth | String |

**Columns (type 16):** `ColumnEntry { id, unknown0, column_name }`. Incorrect
Columns page content causes the player to report "rekordbox database not found".

**Menu (type 17):** `Menu { category_id, content_pointer, visibility, sort_order }`.
Configures browse-menu category visibility and sort order.

**History (type 19):** `History` variant row. Contains history session metadata.

**HistoryPlaylists (type 11) / HistoryEntries (type 12):** Tree structure and
track membership for history sessions, mirroring the playlist_tree /
playlist_entries pattern.

### 2.10.5 DeviceSQL string encoding

Three encodings (rekordcrate `src/pdb/string.rs`):

| Encoding | Condition | Format |
| -------- | --------- | ------ |
| Short ASCII | `len ≤ 0x7F` | `[len:u8][bytes]` |
| Long ASCII | `len > 0x7F`, ASCII chars | `[0xFF][len_high:u8][len_low:u8][bytes]` |
| Long UTF-16LE | `len > 0x7F`, non-ASCII | `[0xFE][len_high:u8][len_low:u8][utf16le_bytes]` |
| ISRC variant | ISRC column only | `[0x80 + len:u8][bytes]` |

### 2.10.6 Producer requirements and hardware constraints

A conforming producer **MUST** honour:

- Page 0 `sequence` **MUST** exceed the maximum `sequence` of all data pages.
  rekordcrate manages this automatically.
- Per-page `sequence = base + (num_rows − 1) × 5` with table-specific `base`
  values (rekordcrate computes these).
- The **Columns** table (type 16) uses a *different* data-page header format from
  other tables; an incorrect Columns page causes the player to report "rekordbox
  database not found".
- **History** pages must be present and populated; players reject an empty
  history even for a fresh export. For an empty export, populate with byte-copied
  rows from a known-good empty database.
- Track and History header pages carry special header bytes (`unknown7` field).
- A single invalid page causes total failure ("database not found"); there is no
  partial-failure mode.

`export.pdb.bak` is a backup copy generated by the player on the device, not by
the exporting producer.

> **TODO (research):** History pages with populated session data have not been
> observed; the empty-history byte-copy strategy is confirmed working for fresh
> exports but populated history requires further capture. [capture]

## 2.11 External Analysis Files (ANLZ)

Detailed beat grids, waveforms, cues, and loops are stored **outside** the
databases, in external ANLZ files — one directory per track. Three files may be
present per track:

- `ANLZ0000.DAT` — legacy analysis: beat grid, mono waveform, and cues.
- `ANLZ0000.EXT` — extended analysis: colour waveforms, extended cues/beat grid,
  and phrase/song-structure data.
- `ANLZ0000.2EX` — Device Library Plus 3-band frequency waveforms.

The `.DAT`/`.EXT` files serve the legacy Device Library; the `.2EX` file serves
Device Library Plus. All three for a given track live in the **same**
hash-derived directory and are referenced by `content.analysisDataFilePath`
(which points at the `.DAT`; sibling paths are derived by substituting the
extension).

### 2.11.1 USBANLZ directory hash

Players **ignore** the stored `analysisDataFilePath` and **recompute** the ANLZ
directory from a hash of the **volume-relative audio path**; a producer therefore
**MUST** place ANLZ files at the hash-derived directory or the player will not
find them (and may re-analyse the track, writing its own files). The directory is
`P<p:03X>/<part2:08X>`. [verified against multiple producers and paths, incl.
paths with non-ASCII characters; hardware-tested on CDJ-3000 fw 3.19]

```python
def compute_anlz_path(file_path: str) -> tuple[int, int]:
    # file_path is the volume-relative audio path: forward slashes, leading '/'
    h = 0
    for ch in file_path:
        c = ord(ch) & 0xFFFF                 # UTF-16 code unit
        h = ((h * 0x5BC9 + c) & 0xFFFFFFFF)
        h = ((h * 0x93B5 + c) & 0xFFFFFFFF)
    part2 = h % 200003                       # 0x30D43 (prime); the 8-hex subfolder
    p  = (part2 >> 0)  & 0x01                 # 7 non-contiguous bits of part2
    p |= (part2 >> 1)  & 0x02
    p |= (part2 >> 4)  & 0x04
    p |= (part2 >> 4)  & 0x08
    p |= (part2 >> 5)  & 0x10
    p |= (part2 >> 8)  & 0x20
    p |= (part2 >> 10) & 0x40
    return p, part2      # dir = f"P{p:03X}/{part2:08X}"
```

Verified path → directory mappings:

| Volume-relative audio path | `P` | `part2` |
| -------------------------- | --- | ------- |
| `/Contents/Charli xcx feat Robyn & Yung Lean/Brat and it's completely different but also stil/01 360-37loy.flac` | `0x039` | `0x000272B9` |
| `/Contents/Justice/✝/02 Let There Be Light.flac` | `0x076` | `0x0001376E` |
| `/Contents/Leo Portela/Bon Vibrant - Leo Portela.flac` | `0x00E` | `0x000281CE` |
| `/Contents/Daniela Cast/Jazzy - Daniela Cast.flac` | `0x00A` | `0x0000CC9C` |
| `/Contents/Huerta/Tatra Motokov - Huerta.flac` | `0x012` | `0x0000530C` |

A producer **SHOULD** also set `content.analysisDataFilePath` (and the legacy
PDB `analyze_path`) to the same hash-derived path for consistency, even though
the player recomputes it.

### 2.11.2 Container format (PMAI) and tags

Each ANLZ file is a big-endian `PMAI` container followed by a sequence of tagged
sections. Every tag section begins with a **12-byte base header**:

| Offset | Size | Field |
| ------ | ---- | ----- |
| `+0` | 4 | Tag identifier (ASCII, e.g. `"PPTH"`, `"PCOB"`) |
| `+4` | 4 | `u32_meta` — tag-specific metadata (see table below) |
| `+8` | 4 | `len_tag_total` (u32) — **full section size** (header + payload) |

The next section starts at `current_offset + len_tag_total`.

**`PPTH` is the only exception:** it has a **16-byte header** with an extra
`path_len` field at offset 12. `path_len = (num_chars + 1) × 2` (UTF-16BE with
NULL terminator).

Tag header sizes and metadata values:

| Tag    | Header size | `u32_meta` | Notes |
| ------ | ----------- | ---------- | ----- |
| `PPTH` | 16 | 16 | Extra 4-byte `path_len` field at offset 12 |
| `PCOB` | 12 | 24 | Cue container |
| `PCO2` | 12 | 20 | Extended cue container |
| `PQTZ` | 12 | 24 | Beat grid |
| `PQT2` | 12 | 56 | Extended beat grid |
| `PWAV` | 12 | 20 | Mono waveform (overview) |
| `PWV2` | 12 | 20 | Mono waveform (small) |
| `PWV3` | 12 | 24 | Colour waveform (preview/scroll) |
| `PWV4` | 12 | 24 | Colour waveform (overview) |
| `PWV5` | 12 | 24 | Colour waveform (detail) |
| `PWV6` | 12 | 20 | 3-band waveform (overview) |
| `PWV7` | 12 | 24 | 3-band waveform (full-resolution) |
| `PVBR` | 12 | 16 | VBR seek table |
| `PVB2` | 12 | 32 | Extended VBR seek table |
| `PSSI` | 12 | 32 | Phrase / song structure |
| `PWVC` | 12 | 14 | Waveform colour summary (rekordbox-only) |

The tag payload size is `len_tag_total − header_size`.

- **`PMAI`** header: 28 bytes — `"PMAI"`, `len_header = 0x1C`, total file size,
  then four words (rekordbox writes `0x01, 0x10000, 0x10000, 0`; some players
  write zeros; both are accepted). [verified]
- **`PPTH`** (file path): stores the same volume-relative audio path as
  **UTF-16 Big Endian with a mandatory trailing `U+0000` (2 bytes)**;
  `path_len = (num_chars + 1) × 2`. Omitting the NULL terminator causes the
  player to silently reject the file and re-analyse. [verified; hardware-tested]

**Empty PCOB / PCO2 containers.** An empty container (no cues) has a
**12-byte payload**: `container_type (u32, 0 = memory, 1 = hot)` + `0x00000000
(u32, declared entry count)` + `0xFFFFFFFF (u32, sentinel)`. `len_tag_total =
24` (12 header + 12 payload). Both memory (type 0) and hot (type 1) containers
are always emitted, even when both are empty. [verified]

Tag set and ordering are **producer-dependent**; a reader **MUST** scan for tags
rather than assume a fixed order. Observed inventories:

| File | Producer | Tags present |
| ---- | -------- | ------------ |
| `ANLZ0000.DAT` | All | `PPTH`, `PVBR`, `PQTZ`, `PWAV`, `PWV2`, `PCOB` (hot), `PCOB` (memory) |
| `ANLZ0000.EXT` | djay Pro | `PPTH`, `PWV3`, `PWV4`, `PWV5`, `PQT2` |
| `ANLZ0000.EXT` | rekordbox | `PPTH`, `PWV3`, `PCOB` (hot), `PCOB` (memory), `PCO2` (hot), `PCO2` (memory), `PQT2`, `PWV5`, `PWV4`, `PVB2`, `PSSI` |
| `ANLZ0000.2EX` | djay Pro | `PPTH`, `PWV6`, `PWV7` |
| `ANLZ0000.2EX` | rekordbox | `PPTH`, `PWV7`, `PWV6`, `PWVC` |

**Minimal viable tag set** (from djay Pro, confirmed working): `.DAT` needs
`PPTH`, `PVBR`, `PQTZ`, `PWAV`, `PWV2`, and both `PCOB` containers (even if
empty). `.EXT` needs `PPTH`, `PWV3`, `PWV4`, `PWV5`, and `PQT2`. `.2EX` needs
`PPTH`, `PWV6`, and `PWV7`. `PSSI`, `PVB2`, `PWVC`, and `PCO2` are **OPTIONAL**
for basic playback. [verified]

### 2.11.3 Cues and loops

Cues and loops live in the ANLZ cue-container tags:

- **`PCOB`** (cue container, `.DAT` and `.EXT`): each container holds one or
  more **`PCPT`** entries. Two containers are emitted per file — one for memory
  cues (`container_type = 0`) and one for hot cues (`container_type = 1`) — and
  both are emitted even when one is empty (see empty format in
  [§2.11.2](#2112-container-format-pmai-and-tags)). [verified]

**PCPT entry layout** (32 bytes per entry, big-endian, from pyrekordbox):

| Offset | Size | Field |
| ------ | ---- | ----- |
| `+0` | 4 | `hot_cue` (u32) — hot cue index (`1`–`8` for hot; `0` for memory) |
| `+4` | 4 | `status` (u32) |
| `+8` | 4 | `Const(0x00010000)` — magic constant |
| `+12` | 2 | `order_first` (u16) |
| `+14` | 2 | `order_last` (u16) |
| `+16` | 1 | `type` (u8) |
| `+17` | 1 | padding |
| `+18` | 2 | `Const(1000)` (u16) |
| `+20` | 4 | `time` (u32) — cue/loop-in point in **milliseconds** |
| `+24` | 4 | `loop_time` (u32) — loop-out point in milliseconds; `0xFFFFFFFF` = no loop |
| `+28` | 4 | padding (16 bytes) |

- **`PCO2`** (extended cue container, `.EXT`): present in rekordbox exports only.
  Holds **`PCP2`** entries (variable length). [verified: times match `PCOB`]

**PCP2 entry layout** (variable length, big-endian, from pyrekordbox):

| Offset | Size | Field |
| ------ | ---- | ----- |
| `+0` | 4 | `hot_cue` (u32) — hot cue index (same encoding as PCPT) |
| `+4` | 1 | unknown |
| `+5` | 3 | padding |
| `+8` | 4 | `time` (u32) — cue/loop-in point in milliseconds |
| `+12` | 4 | `loop_time` (u32) — loop-out point; `0xFFFFFFFF` = no loop |
| `+16` | 1 | `color_id` (u8) — palette index |
| `+17` | 7 | padding |
| `+24` | 2 | `loop_numerator` (u16) — beat-loop length numerator |
| `+26` | 2 | `loop_denominator` (u16) — beat-loop length denominator |
| `+28` | 4 | `len_comment` (u32) — length of comment in bytes |
| `+32` | var | `comment` — UTF-16BE string (`len_comment` bytes) |
| … | 1 | `color_code` (u8) |
| … | 3 | `rgb` (3 × u8) — R, G, B |

Cue times in ANLZ are in **milliseconds**, and memory vs hot is distinguished by
the container type and by the `hot_cue` field (`0` for memory cues, `1`–`8` for
hot cues), consistent with the DB `cue.kind` enumeration
([§2.7.2](#272-kind-cue)).

Real example (a rekordbox track with 6 memory cues, one of which is a loop;
identical values in both `.DAT` `PCOB` and `.EXT` `PCO2`): [verified]

| # | time (ms) | loop end (ms) | kind |
| - | --------- | ------------- | ---- |
| 1 | 257603 | 288826 | memory loop |
| 2 | 210768 | – | memory cue |
| 3 | 148321 | – | memory cue |
| 4 | 132709 | – | memory cue |
| 5 | 10 | – | memory cue |
| 6 | 54651 | – | memory cue |

> **TODO (research):** loops have not been observed in any sample (all
> `loop_time` fields are `0xFFFFFFFF`); the fields documented above are as
> defined by pyrekordbox / crate-digger from real ANLZ data. Populated loop data
> requires a fresh capture with memory/hot loops. [capture]

### 2.11.4 Beat grid

- **`PQTZ`** (`.DAT`, beat grid): header 4 bytes padding + `Const(0x00080000,
  u32)` (required magic) + `entry_count (u32)`, then one 8-byte entry per beat:
  `bar_position (u16be)` + `tempo (u16be, BPM × 100)` + `time_ms (u32be)`.
  [verified]
- **`PQT2`** (`.EXT`): extended beat grid. Header 56 bytes; entry layout defined
  by pyrekordbox.

### 2.11.5 Waveforms

- **`PWAV`** (`.DAT`): 420 bytes = 400 one-byte entries (mono overview). Each
  byte is a height value (`0`–`31`, 5 bits).
- **`PWV2`** (`.DAT`): small mono overview (100 one-byte entries, same height
  encoding as `PWAV`).
- **`PWV3`** (`.EXT`, colour preview/scroll): 1 byte/entry =
  `[color (3 bits)] [height (5 bits)]`. Color index maps to a device-dependent
  palette.
- **`PWV4`** (`.EXT`, colour overview): 6 bytes/entry (Pioneer colour encoding),
  ~1200 entries. Entry layout (from pyrekordbox):
  `[u8, luminance (u8), blue_inv (u8), red (u8), green (u8), blue_height (u8)]`.
  The `u8` at offset 0 encodes `[color_index:3][height:5]`.
- **`PWV5`** (`.EXT`, colour detail): 2 bytes/entry. Entry layout (from
  pyrekordbox): `[R:3][G:3][B:3][height:5][00:2]` where each field is bits
  within a 16-bit big-endian word.
- **`PWV6`** (`.2EX`, Device Library Plus 3-band overview): 3 bytes/entry =
  `[low, mid, high]`; total payload 3608 bytes observed in every file across all
  producers. Entry count matches `PWV4` (~1200). [verified]
- **`PWV7`** (`.2EX`, Device Library Plus 3-band full-resolution): 3 bytes/entry =
  `[low, mid, high]`; entry count matches `PWV3`/`PWV5`.
- **`PWVC`** (`.2EX`): small waveform-colour summary tag. Payload 8 bytes
  observed; rekordbox-only.

> **TODO (research):** the byte-level encoding of `PWV6`/`PWV7` (how each byte
> maps to a display height) and the exact contents of `PWVC` are not established.
> pyrekordbox reads these as raw bytes. Neither pyrekordbox nor crate-digger
> documents the 3-band waveform encoding. [decode]

### 2.11.6 Other tags

- **`PVBR`** (`.DAT`): 1620 bytes; VBR seek table. All zeros for lossless
  (FLAC/WAV/AIFF).
- **`PVB2`** (`.EXT`): extended VBR seek table; zeros for lossless.
- **`PSSI`** (`.EXT`): song-structure / phrase analysis used by newer players'
  phrase features. Header 32 bytes, then 24-byte entries with `mood`, `bank`,
  `index`, `beat`, and `kind` fields. Consistently 356 bytes total in rekordbox
  exports. Entry layout defined by pyrekordbox. [verified]

## 2.12 Player Settings Files

Settings files are **rekordbox-only**. When present, they carry fixed-size binary
blobs read by players. Third-party producers (e.g. djay Pro) omit them and
produce working exports. [verified]

The following files may appear:

- `MYSETTING.DAT` — "My Settings" (player preferences, 4516 bytes observed).
- `MYSETTING2.DAT` — "My Settings" page 2 (1032 bytes observed).
- `DJMMYSETTING.DAT` — DJM mixer settings.
- `DEVSETTING.DAT` — device settings.

Full struct definitions for all four files are provided by pyrekordbox
(`mysettings/structs.py`) using Python `construct` declarative definitions.
Key setting categories include:

- **MYSETTING:** BPM lock, quantize, auto-cue, master-tempo, jog-wheel
  sensitivity and illumination, slip-mode settings, vinyl-speed-adjust, play-mode
  (continue/single), fader-start, language, LCD brightness, hot-cue colouring,
  on-air display, phase-meter, beat-sync, auto-beat-loop, sync-illumination,
  hot-cue-auto-load, and numerous pad/button assignments.
- **MYSETTING2:** MIDI channel, sync-tempo-offset, waveform display preferences,
  brightness, and additional page-2 settings.
- **DEVSETTING:** device-level configuration (body read as raw bytes in
  pyrekordbox).

A non-rekordbox producer **MAY** omit all settings files. A rekordbox-compatible
producer **SHOULD** emit `MYSETTING.DAT` and `MYSETTING2.DAT` populated with
reasonable defaults (copy from a known-good export).

## 2.13 Cached Artwork

Artwork thumbnails referenced by `image.path` are stored under
`.PIONEER/Artwork/NNNNN/...`. Files are standard image files (JPEG observed). The
database `image` row stores only the volume-relative path. [verified]

### 2.13.1 File naming scheme

Two naming conventions have been observed:

- **rekordbox:** writes `aN.jpg`, `aN_m.jpg`, `bN.jpg`, `bN_m.jpg` per track,
  where `N` is the track's artwork index. `_m` variants are smaller thumbnails.
  DB `image.path` always references `bN.jpg`.
- **djay Pro:** writes only `bN.jpg` per track.

A non-rekordbox producer writing only `bN.jpg` files (like djay Pro) produces a
working export. [verified]

### 2.13.2 Folder numbering

Both observed producers place all artwork under `Artwork/00001/`. The allocation
rule for the folder number is not yet determined — single-folder numbering may
be universal or may increment for very large collections.

> **TODO (research):** determine the folder-numbering rule for collections with
> many artwork files. All observed exports (up to 3 tracks) use `00001/`.
> [decode]

## 2.14 `exportExt.pdb` (Extended DeviceSQL Database)

`exportExt.pdb` uses the **same DeviceSQL page format** as `export.pdb`
(little-endian, 4096-byte pages) but a smaller, parallel table set. It ships as
part of rekordbox-produced exports alongside `exportLibrary.db` and
`export.pdb`. djay Pro omits it and produces working OneLibrary-only exports.
[verified]

### 2.14.1 Header and table set

Verified header values: [verified]

```
num_tables = 9, next_unused_page = 21, sequence = 3
```

Table types present:

| Type | Name | Role | Status |
| ---- | ---- | ---- | ------ |
| 0 | Unknown(0) | — not reverse-engineered | Byte-copy |
| 1 | Unknown(1) | — not reverse-engineered | Byte-copy |
| 2 | Unknown(2) | — not reverse-engineered | Byte-copy |
| 3 | Tags | My Tag taxonomy | Full read/write (rekordcrate) |
| 4 | TrackTags | Track-to-tag assignments | Full read/write (rekordcrate) |
| 5 | Unknown(5) | — not reverse-engineered | Byte-copy |
| 6 | Unknown(6) | — not reverse-engineered | Byte-copy |
| 7 | Unknown(7) | — not reverse-engineered | Byte-copy |
| 8 | Unknown(8) | — not reverse-engineered | Byte-copy |

Types 3 (Tag) and 4 (TrackTag) are implemented with full read/write support in
rekordcrate (`src/pdb/ext.rs`). Types 0, 1, 2, 5, 6, 7, and 8 are not
reverse-engineered by any project. For a rekordbox-compatible export, reproduce
these pages byte-for-byte from a known-good empty `exportExt.pdb`.

### 2.14.2 Relationship to Device Library Plus

`exportExt.pdb` carries extended per-entity data that does not fit the legacy
`export.pdb` row layout — notably the My Tag taxonomy (Tag and TrackTag rows)
and extended menu/sort configuration. The Tag table parallels the Device Library
Plus `myTag` table but uses the DeviceSQL storage format.

### 2.14.3 Whether it is required

`exportExt.pdb` is **rekordbox-only**. djay Pro omits it and produces working
exports. Whether legacy hardware requires it for full functionality is unknown
and only checkable on hardware.

> **TODO (research):** types 0–2 and 5–8 are not reverse-engineered. All known
> producers that write `exportExt.pdb` byte-copy these pages. [decode]

## 2.15 Producer Variance

The format admits controlled variation between producers. The following
differences have been observed and are permitted:

| Concern | rekordbox | djay Pro | Guidance |
| ------- | --------- | -------- | -------- |
| Legacy databases (`export.pdb`, `exportExt.pdb`) | Present | Absent | **MAY** omit for OneLibrary-only exports |
| Settings files (`MYSETTING*.DAT`, `DJMMYSETTING.DAT`, `DEVSETTING.DAT`) | Present | Absent | **MAY** omit |
| `category` rows | 22 (includes DATE ADDED) | 21 | Either set is valid |
| `myTag` taxonomy | 28 rows (4 categories + 24 tags) | 0 rows | **MAY** emit empty `myTag` |
| `property.myTagMasterDBID` | Per-master-db value | `0` | Non-rekordbox producers **MUST** set `0` |
| `property.numberOfContents` | Correct count | Always `0` | Readers **MUST** compute from `content` |
| `property.createdDate` | `"YYYY-MM-DD"` | `"YYYY-MM-DD HH:MM:SS"` | Readers **MUST** handle both |
| `property.deviceName` | `""` (empty) | `"rbx"` | Producers **MAY** set freely |
| DDL keyword casing | lowercase | UPPERCASE | Cosmetic; structurally irrelevant |
| Extra tables | None | `djay_migrations`, `djay_content` | Readers **MUST** tolerate |
| `*ForSearch` columns | All `NULL` | Some populated | **MAY** leave `NULL` |
| Artwork files | `aN.jpg`, `aN_m.jpg`, `bN.jpg`, `bN_m.jpg` | `bN.jpg` only | Writing only `bN.jpg` is valid |
| ANLZ `.EXT` tags | Full set (PCOB, PCO2, PSSI, PVB2, PWVC) | Minimal (PWV3/4/5, PQT2) | Minimal set is valid; see [§2.11.2](#2112-container-format-pmai-and-tags) |
| `content.masterDbId` / `masterContentId` | Populated | `0` | Non-rekordbox producers **MUST** set `0` |
| `playlist.sequenceNo` base | 0-based | 1-based | Readers **MUST** use relative ordering |

A reader **MUST** tolerate any combination of these variations. A producer
**MUST** ensure internal consistency regardless of which choices it makes.
[verified]

---

# Part 3 — How-To Guides

*Task-oriented. Each guide solves one concrete problem. Guides are informative.*

## 3.1 Open and Decrypt the Database

**Goal:** obtain a usable SQL connection to `exportLibrary.db`.

### Python (`sqlcipher3`)

```python
import sqlcipher3

KEY = "r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls"

conn = sqlcipher3.connect(".PIONEER/rekordbox/exportLibrary.db")
conn.execute(f"PRAGMA key = '{KEY}'")
conn.execute("PRAGMA cipher_compatibility = 4")  # SQLCipher 4 defaults
assert conn.execute("SELECT count(*) FROM sqlite_master").fetchone()[0] > 0
```

### Node.js (`better-sqlite3-multiple-ciphers`)

```javascript
import Database from 'better-sqlite3-multiple-ciphers';

const KEY = 'r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls';

const db = new Database('.PIONEER/rekordbox/exportLibrary.db', { readonly: true });
db.pragma('cipher = sqlcipher');
db.pragma('legacy = 4');
db.pragma(`key = '${KEY}'`);
```

### Rust (`rusqlite`, `bundled-sqlcipher`)

```rust
use rusqlite::Connection;

const KEY: &str = "r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls";

let conn = Connection::open(".PIONEER/rekordbox/exportLibrary.db")?;
conn.execute_batch(&format!("PRAGMA key = '{KEY}'; PRAGMA cipher_compatibility = 4;"))?;
conn.query_row("SELECT count(*) FROM sqlite_master", [], |r| r.get::<_, i64>(0))?;
```

Open the database through an engine that applies any present `-wal` file.

## 3.2 Read a Track With Its Metadata

**Goal:** resolve a track and its human-readable related fields in one query.

```sql
SELECT c.content_id,
       c.title,
       c.bpmx100 / 100.0            AS bpm,
       c.length                      AS length_seconds,
       c.rating,
       c.path,
       a.name                        AS artist,
       al.name                       AS album,
       g.name                        AS genre,
       k.name                        AS key,
       col.name                      AS color,
       lbl.name                      AS label,
       img.path                      AS artwork_path
FROM content c
LEFT JOIN artist a   ON c.artist_id_artist = a.artist_id
LEFT JOIN album  al  ON c.album_id         = al.album_id
LEFT JOIN genre  g   ON c.genre_id         = g.genre_id
LEFT JOIN key    k   ON c.key_id           = k.key_id
LEFT JOIN color  col ON c.color_id         = col.color_id
LEFT JOIN label  lbl ON c.label_id         = lbl.label_id
LEFT JOIN image  img ON c.image_id         = img.image_id
WHERE c.content_id = ?;
```

Resolve `c.path` and `artwork_path` against the **volume root** to reach the
actual files. `length_seconds` is in seconds.

## 3.3 Read a Track's Cues and Loops

**Goal:** list a track's cue points and loops.

Cues and loops are stored in ANLZ, not in the DB `cue` table (which is empty in
practice; see [§2.6.9](#269-cue)). To read them:

1. Compute the ANLZ directory from the track's `content.path` using the hash in
   [§2.11.1](#2111-usbanlz-directory-hash).
2. Parse `ANLZ0000.DAT` and/or `ANLZ0000.EXT`, scanning for `PCOB`/`PCO2` tags.
3. For each cue record, read the cue time (ms) and loop-end (ms; `0xFFFFFFFF`
   means no loop). The memory container (type 0) holds memory cues/loops; the hot
   container (type 1) holds hot cues/loops.

Convert positions to seconds with `time_ms / 1000.0`.

## 3.4 Walk the Playlist Tree

**Goal:** render the playlist hierarchy and each playlist's tracks in order.

```sql
-- children of a given folder (use = 0 for the root)
SELECT playlist_id, name, attribute, sequenceNo
FROM playlist
WHERE playlist_id_parent = ?
ORDER BY sequenceNo;

-- ordered tracks of a playlist
SELECT content_id
FROM playlist_content
WHERE playlist_id = ?
ORDER BY sequenceNo;
```

Recurse into rows where `attribute = 1` (folders). The same pattern applies to
`history` / `history_content`, `myTag` / `myTag_content`, and `hotCueBankList` /
`hotCueBankList_cue`.

## 3.5 Enumerate History Sessions

```sql
SELECT history_id, name FROM history ORDER BY sequenceNo;

SELECT content_id
FROM history_content
WHERE history_id = ?
ORDER BY sequenceNo;
```

## 3.6 Read Device Properties

```sql
SELECT deviceName, dbVersion, numberOfContents, createdDate FROM property LIMIT 1;
```

Check `dbVersion` before trusting the schema; treat unknown versions
conservatively (see [§5.3](#53-forward-compatibility)). Do not rely on
`numberOfContents`; compute `SELECT count(*) FROM content` for the true value.

## 3.7 Produce or Update an Export

**Goal:** write a valid rekordbox USB export.

1. Lay out the volume per [§2.1](#21-on-storage-layout): audio under `Contents/`,
   everything else under `.PIONEER/`.
2. **Device Library Plus:** create `exportLibrary.db`, setting the SQLCipher key
   **before** creating any tables so the whole file is encrypted from the outset.
   Create the 22-table schema ([§2.6](#26-table-reference)) and the four indexes
   ([§2.8](#28-indexes-and-referential-integrity)). Seed the default catalogues
   ([§2.9](#29-default-catalogues)). Insert reference rows, then `content`, then
   dependent rows and trees. Populate `property` with an accurate
   `numberOfContents` and `dbVersion = "1000"`. Checkpoint the WAL
   (`PRAGMA wal_checkpoint(TRUNCATE)`) before the medium is released.
3. **External analysis:** for each track, write `ANLZ0000.DAT`,
   `ANLZ0000.EXT`, and `ANLZ0000.2EX` at the hash-derived
   `.PIONEER/USBANLZ/P<XXX>/<HHHHHHHH>/` directory
   ([§2.11.1](#2111-usbanlz-directory-hash)), with a NULL-terminated UTF-16BE
   `PPTH` path ([§2.11.2](#2112-container-format-pmai-and-tags)). Cues, loops,
   beat grids, and waveforms go here, not in the DB.
4. **Legacy Device Library (OPTIONAL):** for a complete rekordbox-compatible
   export, write `export.pdb` and `exportExt.pdb`
   ([§2.10](#210-legacy-device-library-exportpdb),
   [§2.14](#214-exportextpdb-extended-devicesql-database)), honouring the
   DeviceSQL page/sequence constraints. A **OneLibrary-only** export (Device
   Library Plus + ANLZ + artwork, without legacy databases) is valid and is the
   configuration produced by djay Pro. See [§2.15](#215-producer-variance).
5. **Settings (OPTIONAL):** if targeting rekordbox compatibility, write
   `MYSETTING*.DAT`/`DJMMYSETTING.DAT`/`DEVSETTING.DAT`
   ([§2.12](#212-player-settings-files)). Otherwise omit.
6. **Artwork:** write cached artwork files (at minimum `bN.jpg` per track)
   under `.PIONEER/Artwork/` ([§2.13](#213-cached-artwork)).
7. Keep both libraries consistent: where both describe the same track, they
   **MUST** describe it identically so playback behaves the same regardless of
   which library a player reads.

---

# Part 4 — Tutorial

*Learning-oriented. A single guided walkthrough. Informative.*

This tutorial opens a real export and prints its playlists and the first track of
each, using Python 3 with the `sqlcipher3` package.

### Step 1 — Point at an export

The Device Library Plus database is at `.PIONEER/rekordbox/exportLibrary.db` on
the exported volume.

### Step 2 — Open and unlock

```python
import sqlcipher3

KEY = "r8gddnr4k847830ar6cqzbkk0el6qytmb3trbbx805jm74vez64i5o8fnrqryqls"

conn = sqlcipher3.connect("<volume>/.PIONEER/rekordbox/exportLibrary.db")
conn.row_factory = sqlcipher3.Row
conn.execute(f"PRAGMA key = '{KEY}'")
```

### Step 3 — Confirm you unlocked it

```python
prop = conn.execute(
    "SELECT deviceName, dbVersion FROM property LIMIT 1"
).fetchone()
count = conn.execute("SELECT count(*) FROM content").fetchone()[0]
print(f"Export '{prop['deviceName']}' v{prop['dbVersion']}: {count} tracks")
```

If this prints without error, decryption worked. (Note that the true track count
comes from `content`, not `property.numberOfContents`.)

### Step 4 — List playlists and a sample track from each

```python
playlists = conn.execute(
    "SELECT playlist_id, name FROM playlist WHERE attribute = 0 ORDER BY sequenceNo"
).fetchall()

for pl in playlists:
    first = conn.execute(
        """
        SELECT c.title, a.name AS artist
        FROM playlist_content pc
        JOIN content c        ON pc.content_id = c.content_id
        LEFT JOIN artist a    ON c.artist_id_artist = a.artist_id
        WHERE pc.playlist_id = ?
        ORDER BY pc.sequenceNo
        LIMIT 1
        """,
        (pl["playlist_id"],),
    ).fetchone()
    sample = f"{first['artist']} — {first['title']}" if first else "(empty)"
    print(f"- {pl['name']}: {sample}")
```

### Step 5 — Clean up

```python
conn.close()
```

You have now decrypted a Device Library Plus export, verified it, and traversed
its playlist structure. From here, consult Part 2 for the full schema and the
ANLZ, legacy, and settings formats.

---

# Part 5 — Conformance

*Normative.*

## 5.1 Conformance Classes

- **Conforming Export:** a volume laid out per [§2.1](#21-on-storage-layout) that
  contains a Device Library Plus database readable per
  [§2.3](#23-encryption)–[§2.9](#29-default-catalogues) and its external analysis
  files placed per [§2.11](#211-external-analysis-files-anlz). A **complete**
  conforming export additionally contains the legacy Device Library
  ([§2.10](#210-legacy-device-library-exportpdb)), `exportExt.pdb`
  ([§2.14](#214-exportextpdb-extended-devicesql-database)), settings
  ([§2.12](#212-player-settings-files)), and artwork
  ([§2.13](#213-cached-artwork)). A **OneLibrary-only** conforming export omits
  the legacy databases and settings files (see [§2.15](#215-producer-variance)).
- **Conforming Reader:** opens and decrypts the database per
  [§2.3](#23-encryption), interprets the schema per
  [§2.6](#26-table-reference)–[§2.7](#27-enumerations), reads cues from ANLZ per
  [§2.11](#211-external-analysis-files-anlz), honours the WAL, computes counts
  directly, and resolves paths per [§2.1](#21-on-storage-layout).
- **Conforming Producer:** produces a Conforming Export, sets
  `dbVersion = "1000"`, satisfies the integrity guidance in
  [§2.8](#28-indexes-and-referential-integrity), places ANLZ files at the
  hash-derived paths with NULL-terminated UTF-16BE `PPTH`, and merges the WAL
  before the medium is released.

## 5.2 Minimum Reader Requirements

A Conforming Reader **MUST** at minimum:

1. Decrypt using the SQLCipher 4 default parameters and the specified passphrase.
2. Read `content` and resolve its artist/album/genre/key foreign keys, treating
   `0` and `NULL` as unset.
3. Traverse the `playlist` tree and `playlist_content`, ordered by `sequenceNo`.
4. Read the `property` row and act on `dbVersion`, computing the track count from
   `content`.
5. Locate and read cues, beat grids, and waveforms from the hash-derived ANLZ
   directory.

## 5.3 Forward Compatibility

The schema is expected to grow. Accordingly:

- Readers **MUST NOT** assume the column set is closed; select columns by name
  and ignore unknown columns and tables.
- Readers **MUST** tolerate producer-added, non-standard tables.
- Readers **SHOULD** treat an unrecognised `dbVersion` as readable on a
  best-effort basis rather than refusing to open.
- Producers **MUST NOT** remove columns defined here, even if unused; populate
  them with sensible defaults (`NULL`/`0`/empty string).
- Readers **MUST NOT** assume a fixed ANLZ tag set or order; scan tags.

## 5.4 Consistency Between Libraries

Where a complete export contains both the legacy Device Library and Device
Library Plus, both **MUST** describe the same content so that a track behaves
identically regardless of which library a given player reads. The shared ANLZ
directories serve both libraries.

---

# Appendices

## Appendix A — Relationship to `master.db`

| Aspect        | `master.db` (desktop)                | Device Library Plus (`exportLibrary.db`) |
| ------------- | ------------------------------------ | ---------------------------------------- |
| Purpose       | Full local library (source of truth) | Export projection for playback           |
| Engine        | SQLite + SQLCipher 4                  | SQLite + SQLCipher 4                      |
| Key           | Raw key `402fd…08497`                 | Passphrase `r8gdd…yqls` (derived via PBKDF2) |
| Table prefix  | `djmd…` (e.g. `djmdContent`)          | unprefixed (e.g. `content`)              |
| Primary keys  | VARCHAR UUID strings                  | Sequential INTEGER                       |
| Table count   | ~47 (incl. cloud-sync/file tables)    | 22 (playback-relevant subset)            |
| Sync columns  | `usn`, `rb_local_synced`, timestamps  | omitted                                  |
| Paths         | Absolute local paths                  | Volume-relative paths (forward slashes, leading `/`) |
| Tempo (BPM)   | Integer × 100                         | Integer × 100 (`bpmx100`)                |
| Duration      | Integer seconds                       | Integer seconds (`length`)               |
| Cue positions | Milliseconds + 1/150 s frames         | Microseconds + 1/150 s frames (DB schema; cues in practice stored in ANLZ in ms) |
| Ratings       | `0`–`5`                               | `0`–`5`                                  |
| Dates         | Timestamps with timezone              | `YYYY-MM-DD` or `YYYY-MM-DD HH:MM:SS` (varies by producer) |

The column semantics of `content` map closely onto `djmdContent`; the main
differences are naming (camelCase vs. PascalCase), the identifier type, and the
omission of synchronisation bookkeeping. `content.masterDbId` and
`content.masterContentId` preserve the link back to the originating master rows
(and are `0` when the producer is not rekordbox).

`master.db` is encrypted with SQLCipher 4 and is unlocked with the raw key
`402fd482c38817c35ffa8ffb8c7d93143b749e7d315df7a81732a1ff43608497` via
`PRAGMA key = "x'402fd...'"` (raw hex key, not PBKDF2-derived). This is
distinct from the Device Library Plus passphrase-based mechanism described in
[§2.3](#23-encryption).

## Appendix B — Glossary

| Term | Definition |
| ---- | ---------- |
| **ANLZ** | External analysis file (`.DAT`/`.EXT`/`.2EX`) holding beat grids, waveforms, and cues. |
| **Device Library** | The legacy DeviceSQL export (`export.pdb`). |
| **Device Library Plus / OneLibrary** | The SQLite/SQLCipher export (`exportLibrary.db`) specified here. |
| **DeviceSQL** | Pioneer's proprietary page-based embedded database engine used by the legacy format and `exportExt.pdb`. |
| **Hot cue bank** | A saved arrangement of hot cues that can be recalled as a group. |
| **My Tag** | rekordbox's user-defined tagging taxonomy. |
| **PMAI** | The container magic and header of an ANLZ file. |
| **SQLCipher** | Transparent AES-256 encryption extension for SQLite. |
| **USBANLZ** | The `.PIONEER/USBANLZ/` directory tree of hash-addressed ANLZ files. |
| **WAL** | SQLite write-ahead log (`-wal` companion file). |

## Appendix C — Supported Hardware (as of 2025)

Device Library Plus is read by newer AlphaTheta / Pioneer DJ players, including
the CDJ-3000X, XDJ-AZ, OPUS-QUAD, and OMNIS-DUO. Older players (e.g. CDJ-3000,
XDJ-XZ) read the legacy Device Library. (AlphaTheta's own guidance confirms the
CDJ-3000X requires Device Library Plus to browse a USB.)

## References

This specification was reconstructed from the following independent
reverse-engineering efforts and documentation. They are the authoritative sources
for byte-level details beyond what is reproduced here.

### Primary format references

- **Deep Symmetry — dysentery / crate-digger** — definitive documentation and
  Kaitai struct definitions for the legacy `export.pdb`, `exportExt.pdb`, ANLZ,
  and settings formats (`rekordbox_pdb.ksy`).
  <https://djl-analysis.deepsymmetry.org/rekordbox-export-analysis/>
- **rekordcrate** (Rust, Holzhaus) — full read/write implementation of the
  DeviceSQL format (`export.pdb`, `exportExt.pdb`) with page allocation, row
  management, and round-trip verification. The canonical implementation for
  DeviceSQL producers.
  <https://holzhaus.github.io/rekordcrate/>
- **pyrekordbox** (Python, dylanljones) — construct-based binary struct
  definitions for all ANLZ file tags (PMAI, PPTH, PCOB/PCPT, PCO2/PCP2,
  PQTZ/PQT2, PWV3/4/5, PSSI, etc.) and player settings files (MYSETTING,
  MYSETTING2, DJMMYSETTING, DEVSETTING). Includes SQLAlchemy ORM models for
  Device Library Plus. Tested against rekordbox 5.8–7.0 with parse-rebuild
  round-trips.
  <https://github.com/dylanljones/pyrekordbox> ·
  <https://pyrekordbox.readthedocs.io/>
- **fourfour / pioneer-usb-writer** — disassembled ANLZ path hash, `PPTH`
  NULL-terminator requirement, byte-level ANLZ/PMAI/PQTZ/PWV* layouts, Device
  Library Plus schema and key, and CDJ-3000 hardware behaviour.

### Encryption and unlocking

- **rbox** (Rust/Python, dylanljones) — implementation for unlocking `master.db`
  and Device Library Plus.
  <https://github.com/dylanljones/rbox> · <https://docs.rs/crate/rbox/>
- **pioneer-rekordbox-database-encryption** (liamcottle) — foundational SQLCipher
  key/PRAGMA research.
  <https://github.com/liamcottle/pioneer-rekordbox-database-encryption>

### Independent implementations

- **DjManager** — independent implementation of the ANLZ directory hash and the
  `.DAT`/`.EXT` section ordering / waveform section list.
- **onelibrary-connect** (TypeScript, chrisle) — reader for `exportLibrary.db`
  (schema and key derivation). <https://github.com/chrisle/onelibrary-connect>
- **alphatheta-connect** (TypeScript, chrisle) — library for AlphaTheta gear,
  incl. network parsing of Device Library Plus.
  <https://github.com/chrisle/alphatheta-connect>
- **opus-quad-pro-dj-link-analysis** (kyleawayan) — mapping of track IDs to
  hardware queries.
  <https://github.com/kyleawayan/opus-quad-pro-dj-link-analysis>
- **rhythmbox-to-pioneer-xdj-exporter** — ANLZ/PDB writing reference.

### Official documentation

- rekordbox Device Library Plus User's Guide (AlphaTheta / Pioneer DJ).
