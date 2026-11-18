# Add-On Content (DLC): Install And Run In Place

Status 2026-11-18: **built and shipped** (launcher 0.5.0, engine `turbo` 8db1151). **Game → Install DLC…**
installs NSP, NSZ, XCI, XCZ and NCA packages; Mario Kart 8 Deluxe's Booster Course Pass (BCP) plays: with the DLC
installed Golden Dash Cup starts and Paris Promenade races, without it the same cup shows the purchase notice.
What was built, what differs from the plan and the verification are in [section 7](#7-what-was-built-2026-11-18);
sections 1 to 6 are the investigation and plan of 2026-11-17, with corrections marked where the build proved them
wrong. The two that matter:

- **The DLC's RomFS is readable** (1.3, 6.1). The ticket is a common ticket, not a console-bound one, so its title
  key decrypts with `titlekek_0c` from `prod.keys`. The RomFS holds `version.txt` and `contents.dat`.
- **A stub storage does not work** (3.1, 6.5). The game reads both files at boot; with the title listed and an
  empty RomFS it hangs on the loading screen after L+R. The real RomFS is installed; nothing is synthesized.

The plan as written on 2026-11-17: Mario Kart 8 Deluxe's Booster Course
Pass (BCP) needs no new data on disk — the 48 course folders are already inside the extracted dump's RomFS —
it needs the *license* to exist at runtime: an `aoc:u` answer that a DLC is installed (the runtime's existing
`aoc:u` hardcodes "none"), plus a mountable AOC data storage for fsp-srv cmd 202. The real AOC NCA on disk
(112 KiB) is titlekey-encrypted and stays locked; the plan synthesizes or pre-extracts that storage
launcher-side instead. Design, work items and verification below.

## 1. What is actually on disk

`P:/Games/Nintendo/DUMPS/Mario Kart 8 Deluxe/` contains three relevant things: the extracted `dump/`
(exefs/ + romfs/, the launcher builds from this), one combined NSP, and one standalone DLC NSP.

### 1.1 The combined NSP (1G+1U+1D)

`Mario Kart 8 Deluxe [0100152000022000][v1507328] (1G+1U+1D).nsp` — PFS0, 20 entries, string table 0x300,
data area base `0x10 + 24*20 + 0x300 = 0x4F0`, file size `0x2D3015914`. Parsed with a 24-byte entry table
(u64 offset, u64 size, u32 nameoff, u32 pad):

| # | name | size | content (from the plaintext cnmt.xml files) |
|---|---|---|---|
| 0–1 | cert + tik (`...2000...00000000`) | 0x700 / 0x2C0 | base ticket |
| 2 | `e1ace5248218fa6393ec1637ce8c2c79.nca` | 0x1AFB58000 (6.74 GiB) | base Program NCA, standard crypto, keygen 0 |
| 3–5 | legal, html, control NCAs | 0x35400 / 0x1704800 / 0x1000 | base |
| 6 | `4ab8e162...cnmt.xml` + `.cnmt.nca` (5) | 0x669 / 0x1000 | base CNMT: Application, v0, PatchId 0100152000022800 |
| 7–15 | update control/html/legal/cnmt (+xml) | 0x12E000 / 0x700 / 0x2C0 / 0x2E200 / 0x173600 / 0x121016600 / 0xA7AC00 / 0x2600 / 0x67B | update CNMT: Patch, **v1507328**, Program `9b5ca186...` 4,848,707,072 B, KeyGeneration 4 |
| 16–19 | **DLC tik, cert, `1fa7f203...cnmt.nca`, `a3e030a3...nca`** | 0x2C0 / 0x700 / 0xE00 / 0x1C000 | title 0100152000023001, see 1.3 |

**The container is fully valid.** Every recorded offset chains exactly (each entry begins where the previous one
ends), and entry 19's end + `0x4F0` lands exactly on EOF (`0x2D3015424 + 0x4F0 = 0x2D3015914`). An earlier parse
of this container reported "corrupt offset fields" on entries 2 and 12 — that was a **parser bug, not a
container bug**: the PFS0 header is **16 bytes** (magic, count, string-table size, reserved), and reading it as
24 shifts the entry table by 8, so each entry's *size* field lands in the *offset* slot and a perfectly good
container looks corrupted. The standalone DLC NSP re-verified the same way (4 entries, data base `0x10A`, ends
exactly at EOF). Any table-driven PFS0 parser in this project must start entries at `0x10`.

### 1.2 The update carries the courses

The update's Program NCA `9b5ca186...` (4,848,707,072 B) is **titlekey-encrypted**
(`rights_id = 01001520000228000000000000000004`, keygen 4 → master key 3). Its section table:

| sec | media | size | type |
|---|---|---|---|
| 0 | `0x11FEBE600` | 17.4 MiB | ExeFS patch (PartitionFS, CTR) |
| 1 | `0x4000` | ≈4,593 MiB (extent from the BKTR superblock; its section entry fields are placeholders) | **RomFS patch — IVFC + BKTR** (`IVFC` magic at FsHeader+0x08; relocation block `@0x11FE92400 + 0x18000`, subsection table `@0xE40 + 0x11FEAA400`) |

The BCP course data lives in that BKTR patch RomFS. Proof it landed on the user's console dump:
`dump/romfs/Course/` contains **48 `Cnsw_*` course folders** (the 48 BCP tracks) plus the BCP battle courses
(`Bu_*` ×5, `Bgc_*`, `Bsfc_*`) — 1,238 files. The extracted dump the launcher builds from already has every
track.

### 1.3 The DLC title is a 112 KiB license stub

Both the standalone NSP and entries 16–19 of the combined one contain the same four objects (SHA-256
identical; standalone data base `0x10A`):

| file | size | verified content |
|---|---|---|
| `0100152000023001000000000000000d.tik` / `.cert` | 0x2C0 / 0x700 | rights id ...000d |
| `1fa7f2036e3aabdede91fa1cd78705ca.cnmt.nca` | 0xE00 | NCA3, Meta, keygen 13 → master key **12**, title **0100152000023001**, SDK 14.2.0.0; PFS0 at media 0xC00 (+0x20 body) holding `AddOnContent_0100152000023001.cnmt`: **type 0x82 (AddOnContent), version 65536, 1 content record** = `a3e030a30e112d39de059a24f7c80b63` |
| `a3e030a30e112d39de059a24f7c80b63.nca` | 0x1C000 (112 KiB) | NCA3, Data (ctype 5), **titlekey-encrypted** (`rights_id = 0100152000023001000000000000000d`); one RomFS section (ptype 0, fstype 3, CTR) at media `0x4000`, size `0x18000` (96 KiB) |

The 96 KiB RomFS could not be read here: the ticket's RSA-OAEP titlekey decrypts only with the *issuing
console's* `eticket_rsa_keypair`, and neither OAEP-SHA256/SHA1 nor PKCS#1 v1.5 at any block offset of the
ticket decoded with the local keypair (open question 6.1). **Neither NSP contains the tracks** — the DLC
distribution model is "update ships the data, DLC title is the license + a small stub".

> **Correction (2026-11-18).** The RomFS is readable. The ticket's title key type byte (`+0x281`) is 0: a *common*
> ticket. Its title key block (`+0x180`) is a 16-byte key followed by zeros, wrapped with `titlekek_0c` (master key
> revision 13), not an RSA block for one console, which is why no RSA scheme decoded it. hactool
> `--titlekey=<those 16 bytes>` decrypts the section. The RomFS (IVFC level 6, 0x1290 bytes) holds two files:
> `version.txt` (5 bytes: `10002`) and `contents.dat` (4096 bytes, high entropy). The game reads both at boot.

All of the above was decrypted in memory only (no content extracted to disk) with hactool 1.4.0 cross-checked
byte-for-byte; the crypto notes that matter for later tooling: hactool derives the header key from
`master_key_00` + sources (`pki.c` "Derive Header Key") rather than trusting the file's `header_key` line, and
the NCA XTS tweak is Nintendo's custom one — sector number bytes **8..15, reversed** (`aes.c get_tweak`),
tweaks relative to the NCA start, and section CTR IV = 8 zero bytes + big-endian `media_offset>>4` in bytes
8..15.

### 1.4 MK8D references DLC at runtime

`dump/exefs/sdk` (the NintendoSDK module, 4.4 MB) contains the service string `aoc:u` (offset `0x370393`) and
`nn::fs::MountAddOnContent(char const*, ...)` (offset `0x325E05`); `main` has the game string
`ShowShopAddOnContentList` (`0x8062F3`). So MK8D links the AOC API: it enumerates DLC through `aoc:u` and
mounts each add-on content's RomFS as a data storage. Today the runtime already registers `aoc:u`
(`runtime/svc_core.c:1258`) but answers "no DLC", which is why BCP cups don't appear.

## 2. How Eglantine (yuzu fork) does it

### 2.1 Install

Two storage mechanisms, both keep content in place (NCAs are never expanded):

- **NAND registered cache** — `BISFactory` scans `<nand>/system/Contents/registered` and
  `/user/Contents/registered` (`file_sys/bis_factory.cpp:23-26`). `RegisteredCache` walks the console layout
  `registered/000000XX/<ncaid>.nca` (`registered_cache.cpp:659-693`), builds one `NCA` per file, and for each
  Meta-type NCA stores `title_id → CNMT` (`ProcessFiles`, `registered_cache.cpp:695-718`). A `(title_id,
  ContentRecordType)` lookup resolves through that CNMT's content records to the NCA file
  (`GetNcaIDFromMetadata` / `GetEntryRaw`, `registered_cache.cpp:606-657, 776-810`). The write path is the
  console scheme: `PlaceholderCache::Create/Write/Register` (`registered_cache.cpp:388-530`).
- **Manual content provider** (what the game list uses) — an NSP/XCI is parsed and each CNMT's content records
  are registered as in-memory `(TitleType, ContentRecordType, title_id) → VirtualFile` pairs
  (`ManualContentProvider::AddEntry`, `registered_cache.cpp:1251-1320`; populated from the game list at
  `qt_common/game_list/worker.cpp:450` and `qt_common/util/content.cpp:570-580`). Updates additionally record
  their CNMT version (`AddEntryWithVersion`, `registered_cache.cpp:1313-1315`).

Identification is entirely CNMT-driven: `TitleType::AOC = 0x82` (`nca_metadata.h:22-33`), the content record
type for AOC payloads is `ContentRecordType::Data = 2` (`nca_metadata.h:35-44`, records are 0x38 bytes:
hash32 ‖ nca_id16 ‖ size6 ‖ type1 ‖ id_offset1, `nca_metadata.h:46-53`). Updates vs DLC is just the CNMT
type byte: Update 0x81, AOC 0x82. Title id math (`file_sys/common_funcs.h:10-53`): base = `tid & ~0x1FFF`,
update = base + 0x800 (`registered_cache.h:63`), AOC base = base + 0x1000, AOC id = `tid & 0x7FF`.
CNMT parsing reads records at `sizeof(CNMTHeader)=0x20 + header.table_offset + i*0x38` — the 0x10-byte
`table_offset` (the "optional header") exists for Application/Update/AOC (`nca_metadata.cpp:16-42`,
`nca_metadata.h:72-86`), which is exactly the offset empirically confirmed in 1.3.

### 2.2 Runtime application

The AOC RomFS is **not** merged into the program RomFS. The game does three things, all served from the
content provider:

1. `aoc:u` enumeration — `CountAddOnContent` / `ListAddOnContent` (`hle/service/aoc/addon_content_manager.cpp:99-145`)
   count/return the AOC entries owned by the caller's base title:
   `AccumulateAOCTitleIDs` filters `ListEntriesFilter(TitleType::AOC, ContentRecordType::Data)`
   (`addon_content_manager.cpp:42-58`) and drops entries whose NCA fails to load; List returns
   `GetAOCID(title_id)` as u32. `GetAddOnContentBaseId` (`:147-164`) prefers the control NACP's
   `GetDLCBaseTitleId()`, falling back to `GetAOCBaseTitleID`.
2. Mounting — `nn::fs::MountAddOnContent` ends in fsp-srv
   `OpenDataStorageByDataId` (`hle/service/filesystem/fsp/fsp_srv.cpp:511-542`): the DLC title id is resolved
   through `RomFSFactory::GetEntry` by storage id (`file_sys/romfs_factory.cpp:87-113`) → `NCA::GetRomFS()`.
3. Updates merge only into the *program* RomFS: `PatchManager::PatchRomFS`
   (`file_sys/patch_manager.cpp:732-858`) takes base_romfs, builds `NCA(update_raw, base_nca)` and uses its
   RomFS (order: base → update → LayeredFS mods; the update wins on conflicts because the BKTR layer sits on
   top). The BKTR machinery is in the `NCA` constructor
   (`file_sys/content_archive.cpp:32-119`): an `AesCtrEx` section marks an update NCA
   (`:109-115`), a base NCA must be supplied (`ErrorMissingBKTRBaseRomFS`, `:114-115`), and titlekey
   encryption is handled by reading the ticket titlekey from the key manager and decrypting it with the
   titlekek for the master key id (`:53-78`).

Switch 2 specifics: nothing in the DLC path is nx2-specific (the lookup is by title id and storage id only);
`control_metadata.h:251` notes Switch 2 upgrade packs are themselves distributed as AddOnContent titles, which
doesn't affect a Switch 1 title. MK8D is a Switch 1 program throughout.

## 3. Recommended design for RecompLauncher + Switch2RECOMP

### 3.1 What DLC means here (the model)

DLC is a *license plus a separate data storage*, never a RomFS overlay. So neither of the two obvious
architectures is the right one:

- **(A) "launcher extracts the DLC RomFS, runtime layers it over rom:"** — rejected. Switch games never read
  DLC through `rom:`; layering would put files into paths the game never opens, and MK8D's courses are already
  in `rom:` from the update (section 1.2). Extraction of the real AOC NCA is additionally blocked by the
  titlekey (1.3).
- **(B) "runtime opens the NSP in place and layers the AOC NCA"** — rejected. The runtime's NSP support
  (`runtime/nsp.c:15-26`) reads a *plaintext* byte range recorded by the build (`nsp.py marker()`,
  `third_party/Switch2RECOMP/nsp.py:228-229`); it has no NCA crypto at all, so a titlekey-encrypted AOC NCA is
  unreachable from the runtime.

- **(C) Recommended: launcher registers DLC as metadata + optional extracted folder; runtime serves the
  license and the storage.** Concretely:
  1. *License*: the launcher writes a per-game DLC manifest into the build's data dir; the runtime's `aoc:u`
     reads it and answers Count/List/AddOnContentBaseId/Prepare from it. For MK8D the manifest is one line
     naming title id `0100152000023001` (AOC id 1).
  2. *Data storage*: fsp-srv cmd 202 (`runtime/svc_fs.c:491`) learns to return a RomFS IStorage for a DLC data
     id. Source order mirrors the existing RomFS source order in `romfs_init` (`runtime/svc_fs.c:128-147`):
     a real extracted RomFS folder `<Game>_data/dlc/<titleid16>/romfs/` when the launcher could decrypt it
     (hactool + ticket titlekey at build time), else a synthesized valid RomFS image (the `synrom_build`
     machinery already in `svc_fs.c:361-378`, same spirit as MiiModel at `svc_fs.c:429-455`).
  3. MK8D needs no new game data: the tracks are in the build's `romfs/` already; the AOC storage only has to
     mount. Start with the synthesized stub and a probe run, and promote to the real extracted RomFS if the
     game reads specific files from it.

This matches Eglantine's semantics (2.2) with the smallest possible surface, reuses existing runtime
machinery (synthesized storages, folder-built RomFS images), and works without decrypting anything new.

### 3.2 library.json / launcher changes

`Game` (`src/launcher/library.h:22-40`, serialized at `library.cpp:232`) gains one list field,
`dlc` (array of absolute NSP paths). Nothing else in the schema changes; `builtSource`/`hasBuild` logic is
untouched. At build time (and again on "Play" if the manifest is missing), the launcher writes
`<buildDir>/<outputName>_data/dlc.manifest`, one entry per DLC:

```
# <title id 16 hex> <version decimal> <source path>
0100152000023001 65536 P:/Games/.../Booster Course....nsp
```

and, when it can, extracts the AOC RomFS to `<...>_data/dlc/<titleid16>/romfs/`. Extraction is a build step in
the engine, not the launcher: the engine already has hactool + keys plumbing (`switchrecomp.py` NSP builds get
`SWR_PROD_KEYS`, `library.cpp:171`; `buildmanager.cpp:48-50` gates NSP builds on keys). A new `switchrecomp.py
--install-dlc <nsp> --datadir <data dir>` (or the DLC list passed alongside the game source) would: parse the
container (16-byte PFS0 header — section 1.1), read the meta NCA's CNMT via hactool, pick the Data record, decrypt
it with the ticket titlekey (hactool `--titlekey`), and extract its RomFS. When decryption fails (console-locked
ticket, like ours), it writes the manifest anyway with a `stub` marker; the runtime then synthesizes.

### 3.3 Runtime changes

- `aoc:u` (`runtime/svc_core.c:1128-1135`): load `<datadir>/dlc.manifest` once at first call; cmd 2
  (CountAddOnContent) returns the count of entries whose title id matches `GetBaseTitleID(g_title_id)`;
  cmd 3 (ListAddOnContent) writes the u32 AOC ids (`tid & 0x7FF`) into the output buffer
  (`buf_out`/`GP`, respecting offset/count args) and returns the served count; cmd 5/6 already return the
  right base id (`svc_core.c:1131`); cmd 7 (PrepareAddOnContent) already returns success. Without a manifest
  the behavior stays exactly as today (0).
- fsp-srv cmd 202 (`runtime/svc_fs.c:491-494`): before the `sysdata_open`/empty fallback, resolve the data id
  against the DLC manifest; if a real `dlc/<id>/romfs/` folder exists, build a RomFS image from it (the
  `romfs_build`/`romfs_emit` machinery at `svc_fs.c:71-105` operating on a second tree) and hand out a
  `synstorage`-style IStorage (`svc_fs.c:380-388`); else if the manifest marks the id as `stub`, hand out a
  small synthesized RomFS image and log reads (per-offset logging of the first reads gives the file-name
  recovery path if the game looks for specific files — the same trick that drove MiiModel).
- No ABI change: `runtime/cpu.h` untouched; a runtime rebuild + `switchrecomp.py` re-copy of the EXE suffices
  (MK8D doc §"How to reproduce").

### 3.4 Versioning

DLC version 65536 vs game update 1507328: on a console the pairing is enforced by the update CNMT's
`RequiredApplicationVersion` / NACP metadata, not by the DLC. Nothing in the runtime needs to compare them;
the launcher records the DLC version in the manifest (display + "update the game first" warning if the build's
source is older than the DLC's requirement, only worth adding if a real case shows up). MK8D's BCP stub is
v65536 on top of update v1507328 and the dump's RomFS is the 3.0.x content — consistent pair, nothing to do.

## 4. Step-by-step work items (ordered)

| # | Work item | File(s) |
|---|---|---|
| 1 | `dlc.manifest` loader in the runtime: parse `<datadir>/dlc.manifest` (title id, version, source path) into a small table; helper `dlc_for_base(u64 base)` | `runtime/svc_fs.c` or a new `runtime/dlc.c` + `rt.h` |
| 2 | `aoc:u` Count/List from the manifest (Count → n; List → write `tid & 0x7FF` ids into the buffer, honor offset/count); keep today's behavior when no manifest | `runtime/svc_core.c:1128-1135` |
| 3 | fsp-srv cmd 202: manifest hit + `dlc/<id>/romfs/` folder → build+serve a RomFS IStorage; manifest `stub` → synthesized RomFS; log every distinct read for the first N reads | `runtime/svc_fs.c:491-494`, reuse `svc_fs.c:71-105, 361-388` |
| 4 | Probe run: MK8D + manifest present, stub storage. Watch the log for `aoc` traffic, cmd-202 reads, and whether the BCP cups appear on the title screen; record any file names the game requests from the AOC storage | manual run + `docs/MK8D_BOOT_INVESTIGATION.md`-style notes |
| 5 | If (4) shows named requests: synthesize those files (or hand-place them in `dlc/<id>/romfs/` from the extracted dump if they turn out to be copies of RomFS content); re-run | `runtime/svc_fs.c` |
| 6 | Engine: DLC-aware NSP container reader — parse the 16-byte PFS0 header correctly (a 24-byte read misreads size fields as offsets, 1.1), parse meta NCA CNMT via hactool, extract Data-record RomFS with ticket titlekey into `<data>/dlc/<id>/romfs/`; on titlekey failure write `stub` into the manifest | `third_party/Switch2RECOMP/nsp.py` (or new `dlc.py`), `switchrecomp.py` (`copy_data` neighbor, `:412`) |
| 7 | CLI + launcher plumbing: `--install-dlc` (engine), `Game.dlc` field + manifest writer at build and on Play (launcher) | `switchrecomp.py`, `src/launcher/library.{h,cpp}`, `src/launcher/buildmanager.cpp`, `src/recomp_cli.py` |
| 8 | Tests: synthetic DLC NSP (tik/cert/meta/content stub, plaintext Data NCA) through the engine extractor; runtime unit test for the manifest + aoc List buffer fill; a PFS0 header-size regression case (entries must read at `0x10`) | `third_party/Switch2RECOMP/tests/`, `runtime` test harness |
| 9 | Docs: fold results back into `docs/DLC_IN_PLACE.md` and `NSP_IN_PLACE.md` (PFS0 header is 16 bytes; both local NSPs verified valid end-to-end) | docs |

Steps 1–5 alone make MK8D's BCP playable-cups-visible (license + mountable storage, no new data). Steps 6–8
are the general path for DLC that ships real content.

> **Outcome (2026-11-18).** All nine items are done; step 4's probe sent step 5 another way (the files are the
> title's own, so the installer of step 6 provides them, nothing is synthesized). Where each landed is in
> [section 7](#7-what-was-built-2026-11-18).

## 5. Verification

- **Log lines to expect**: `aoc:u` Count answered 1; fsp cmd 202 for `0100152000023001` served (log its size —
  0x200 for the stub image); the existing `romfs:` per-file logs unchanged.
- **In game**: MK8D title screen → the Booster Course Pass cups (waves) appear in cup select; a BCP course
  (e.g. `Cnsw_11`, Paris Promenade) starts and renders. The course data itself is plain RomFS reads —
  identical mechanics to the base courses already proven in `MK8D_BOOT_INVESTIGATION.md`.
- **Negative test**: delete `dlc.manifest` → Count 0 → BCP cups hidden, game still boots (today's behavior).
  (As run on 2026-11-18: the game boots as before, but the cups are not hidden. MK8D lists the BCP cups for
  everyone and answers a non-owner with the purchase notice; see 7.5.)
- **Regression**: MKW boots as before (it queries neither `aoc:u` nor cmd 202 today); engine tests
  `test_n*.py` stay green; no ABI bump, so a runtime-only redeploy suffices.
- **Cannot be determined without running** (probe items for step 4): whether MK8D calls `PrepareAddOnContent`
  (cmd 7) before mounting, which file names (if any) it expects inside the AOC storage, and whether the shop
  UI path (`ShowShopAddOnContentList`) errors visibly without an `es`/purchase-event backend — the purchase
  event manager is already stubbed (`svc_core.c:1126`).

## 6. Open questions / risks

1. **The AOC RomFS content is unreadable here.** The ticket's titlekey is RSA-OAEP-encrypted to the issuing
   console's `eticket_rsa_keypair`; the local keypair does not decode it (OAEP-SHA256/SHA1 and PKCS#1 v1.5
   tried at every block offset of the ticket). Consequence: the real 96 KiB RomFS stays sealed; the plan does
   not depend on it (stub-first design). If the game rejects a stub, the next options are: re-dump the ticket
   on the owning console, or feed the RomFS extracted by a tool that has the right keypair.
2. **License checks beyond aoc:u.** MK8D may cross-check the ARP/application record or es rights at some UI
   path. Eglantine stubs these and games tolerate it; our runtime has no `arp` service at all today and the
   game already boots, so treat as verify-if-it-bites.
3. **Save data**: BCP completion flags live in the existing MK8D save. The investigation doc's save-backup
   rule (open item 8) applies to every probe run.
4. **Container parsing starts at `0x10`.** What looked like corrupt entry offsets (1.1) was a 24-byte header
   read in two independent parsers; both local NSPs verified valid, so the runtime's direct-NSP path
   (precomputed byte ranges written by the build, not table reads) is unaffected. Any new table-driven parser
   must use the 16-byte PFS0 header.
5. **Stub-first vs real-content mismatch**: if MK8D verifies files inside the AOC storage (hashes/sizes), a
   synthesized stub won't survive; that's what step 4's probe decides before step 6 is built.
6. **Multi-DLC games** (future): the manifest design already lists several entries; ListAddOnContent ordering
   (by id_offset, per Eglantine's record order) should be kept stable so games index them predictably.

> **Outcome (2026-11-18).** 1: wrong, the ticket is a common one and the RomFS decrypts (correction in 1.3). 2: nothing
> bit; `aoc:u` and the data storage are all MK8D asks for. 3: the save was backed up before the runs and restored
> after them. 4: `dlc.py` reads PFS0 tables at 0x10 and a test pins it. 5: confirmed the hard way, a stub hangs the
> game, so a title is installed with its data or not at all. 6: the manifest and the List answer are sorted by title
> id; a package with several titles is covered by a test, no real multi-DLC game was tried.

## 7. What was built (2026-11-18)

### 7.1 In the launcher

- **Game → Install DLC…** opens the selected game's **DLC window** (`DlcDialog` in `dlcpanel.cpp`, wired in
  `window.cpp`): one row per package with the title ids and versions installed from it
  (`Booster Course [...].nsp — 0100152000023001 v65536`).
  - **Add DLC…** (or a drop on the window) takes any number of `*.nsp *.nsz *.xci *.xcz *.nca` files at once. Each
    file's type and signature are checked (`validDlcPackage`), the paths join `Game.dlc` in `library.json` (an array
    of absolute paths, as in 3.2), and the engine installs them into the build's data folder. What the engine
    refuses is shown under the list with its reason and leaves the list again.
  - **Remove** uninstalls the selected rows, several at once: they leave the list and the data folder right away.
  - A game with several DLC packages, and a package with several add-on titles, are both handled: every title is
    installed on its own, and a title that two packages hold stays with the one installed first (the other is
    refused with that package's name) unless the new one is a newer version.
- **Properties → DLC** (`DlcPanel`): the same list. Right-click → **Remove** or Delete; **OK** applies it and
  the engine deletes the title from the data folder.
- **When the data folder is written**: right after Install DLC… for a built game, after every successful build, and
  before **Play** whenever the folder does not hold exactly the listed packages (`dlcUpToDate`). All three run the
  same engine command, `switchrecomp.py --sync-dlc <packages> --datadir <build>/<name>_data` (`DlcInstaller` in
  `buildmanager.cpp`); its output goes to the build console.
- DLC found in the data folder but missing from `Game.dlc` (installed from the command line, or the library file
  was written by an older launcher since) is added to the list when Properties or Play looks at the game.
- `RecompLauncher --select-title <game> --install-dlc <package>` and `--remove-dlc <package>` are the window's Add
  and Remove without dialogs (exit code 0 or 1); `--show-menu game`, `--show-dlc` and
  `--show-properties --properties-tab 4` with `--screenshot` show the new UI.
- Releases ship the engine's `dlc.py` (`scripts/package-release.py`).

### 7.2 In the engine

Reference: `third_party/Switch2RECOMP/docs/DLC.md`.

- `dlc.py` and `switchrecomp.py --install-dlc / --remove-dlc / --sync-dlc / --list-dlc --datadir <data>`.
- `runtime/dlc.c`: the manifest, the `aoc:u` Count / List answers (`svc_core.c`), and the title storages behind
  fsp-srv 202 (`svc_fs.c`). No ABI change.
- Differences from the plan in section 3:
  - The manifest line has a storage word: `<title id> <version> <data|stub> <package>` (the three-column form is
    still read).
  - The title's RomFS is stored as an **image**, `dlc/<title id>/romfs.bin` (exactly the bytes the game would get),
    not as an extracted folder. A plaintext RomFS is not copied at all: `dlc/<title id>/romfs.nsp` is a marker and
    the runtime reads the range from the package, like a game's own `romfs.nsp`. A hand-filled
    `dlc/<title id>/romfs/` folder is served too.
  - No stub: a package whose data cannot be read is refused with the reason. The runtime still understands a
    `stub` line (an empty RomFS) and `--dlc-stub` writes one, for diagnostics only.
  - The engine writes the manifest, not the launcher; the launcher only passes the package list.
  - A title that is in the manifest but whose data is not there when the game starts (the package a marker points
    to was moved, the image was deleted) is not offered to the game, with a warning in the log, because of what a
    title without data does to MK8D (7.5).

### 7.3 Formats, against Eglantine's list

Eglantine's "Install Files to NAND" filter is `*.nca *.nsp *.nsz *.xci *.xcz` (`yuzu/main_window.cpp`
`OnMenuInstallToNAND`); its external content scan takes `nsp nsz xcz xci`.

| Format | Eglantine | Here |
|---|---|---|
| `.nsp` | yes | yes: title-key (common ticket), key-area and plaintext data NCAs |
| `.nsp` holding game + update + DLC (1.1) | yes | yes: only the add-on titles are installed (verified with the 11.3 GiB 1G+1U+1D package) |
| `.nsz` | yes (block and, for install, solid NCZ) | yes: block and solid NCZ, decompressed with the Python the engine already uses for NUP (3.14, `backports.zstd` or `zstandard`) |
| `.xci` | yes | yes: the secure partition, with or without the dump's key area |
| `.xcz` | yes | yes |
| `.nca` | yes (asks for the title type) | yes for an add-on Data NCA; with a rights id it needs `<rights id>.tik` beside it |

Not installable anywhere, and refused here with the reason instead of being accepted: a ticket personalized for
another console, data whose ticket is missing, keys without the content's key generation. On Linux the engine needs
`hactool` on the PATH, as for NSP builds. Only the two real MK8D packages were available as real data; NSZ, XCI,
XCZ and bare NCA are covered by synthetic packages in the engine tests.

### 7.4 What Mario Kart 8 Deluxe does

At 3.4 s, before the title screen (log of the final run):

```
dlc: 1 add-on content title in ...Mario Kart 8 Deluxe 1G1U1D_data/dlc.manifest
aoc: ListAddOnContent offset 0 count 16 -> 1
aoc: PrepareAddOnContent 1
fs: add-on content 0100152000023001 (image, 4.6 KB)        (mounted twice)
dlc: 0100152000023001 read 0 +50                           RomFS header
dlc: 0100152000023001 read 1208 +c ... 1238 +58            directory and file tables
dlc: 0100152000023001 read 1200 +5                         version.txt
dlc: 0100152000023001 read 200 +1000                       contents.dat
```

So the answers to section 5's "cannot be determined without running": the game does call `PrepareAddOnContent`
(index 1) before mounting; it wants `version.txt` and `contents.dat`; no `es` or purchase backend was needed.

### 7.5 Verification

Real library, real build folder (`.../SwitchRecomp Builds/Mario Kart 8 Deluxe 1G1U1D`), harness
`tools/plza_probe.py`. The game was rebuilt through the CLI with the launcher's arguments first (the game's exe hash
equals the engine's `bin/runtime_win64_win32.exe`), the save was backed up before and restored after.

| Run (`D:/Projects/Switch2RECOMP-main/build/evidence/dlc/`) | Data folder | Result |
|---|---|---|
| `20261118-010020-mk8d-no-dlc` | no manifest (the negative test) | boots as before, `ListAddOnContent ... -> 0`, main menu without the shop check mark (`0004`). The second cup page lists the 12 BCP cups for everyone (`0012`), but Golden Dash Cup answers "You can play the new cups by purchasing the Booster Course Pass..." (`0013`) and returns to cup select |
| `20261118-002506-mk8d-dlc-stub` | title listed, empty RomFS | title screen, then L+R: loading screen for a minute until the run was stopped |
| `20261118-002837-mk8d-dlc-folder` | first probe with the real files (a hand-filled `romfs/` folder) | menus, BCP page, Golden Dash Cup, Paris Promenade driven |
| `20261118-010258-mk8d-dlc-bcp` | **installed by the launcher** from the standalone NSP (`romfs.bin`) | main menu with the shop check mark (`0002`), cup select (`0010`), "Start Grand Prix?" for Golden Dash Cup (`0011`), Paris Promenade (`Course/Cnsw_11`) driven, 3rd under the Eiffel Tower (`0014`, `0015`), 56 FPS over the run, no crash |
| `20261118-011120-mk8d-dlc-data-missing` | the same manifest, `romfs.bin` moved away for the run | log: `dlc: 0100152000023001 is installed but its data is missing ... not offered to the game`, `ListAddOnContent ... -> 0`; L+R goes on to Select a Mii (`0001`) instead of hanging |
| `20261118-011210-mk8d-dlc-final` | the launcher-installed DLC again, with the runtime as shipped (engine 2cdeabd, the game re-placed through the CLI) | Select a Mii, main menu, Golden Dash Cup, Paris Promenade driven, 2nd place (`0015`), 60 FPS over the run, no crash |

- The combined 1G+1U+1D package installs the same title through the CLI (`--install-dlc`), 4.6 KB image, and the
  same file again is reported as already installed.
- Launcher UI (own instance, real library): `launcher/game-menu.png` (Game menu with Install DLC…),
  `launcher/dlc-window.png` (the DLC window listing the Booster Course package), `launcher/properties-dlc.png`
  (Properties → DLC).
- Remove and several packages, through the launcher on the real library (`--remove-dlc`, `--install-dlc` twice):
  removing the Booster Course package deleted the manifest and `dlc/` and emptied `Game.dlc`; removing it again
  failed (exit 1); installing the standalone package and the combined 1G+1U+1D package together installed the title
  from the first and refused the second ("already installed from ..."), leaving one entry in the list. The
  packages' hashes are unchanged.
- Regression: Mario Kart World rebuilt through the CLI with the same runtime (exe hash equal) and run to character
  select at 60 FPS, no crash (`20261118-013425-mkw-dlc-regress`). Its only add-on call is `aoc:u` 50
  (CheckAddOnContentMountStatus); it lists nothing and mounts no data storage. Its save did not change.
- Tests: launcher `launcher-tests` and `launcher-ui-tests` (the DLC list in `library.json`, the manifest reader,
  package checks, the installer process, the Game menu, the Properties list); engine `tests/test_nsp_dlc.py`
  (20 cases, section 6 of the engine's `docs/DLC.md`) inside `unittest discover -s tests -p "test_n*.py"` (37 OK).

### 7.6 Open

- The title screen shows "Ver. 1.0.0": the runtime's display version, unrelated to DLC.
- BKTR and AES-XTS data sections are refused; no add-on content with either was seen.
- ~~Building a game straight from a combined NSP (game + update in one file) is not supported.~~ Since launcher
  0.5.2 it is: the update's BKTR patch is applied in place, and a `1G+1U+1D` package joins the game's DLC list on
  the first build ([NSP_IN_PLACE.md](NSP_IN_PLACE.md)). A game built that way also gets its real display version
  instead of "Ver. 1.0.0"; a game built from an extracted dump shows the version of a `control.nacp` placed in the
  dump folder, else still 1.0.0.
