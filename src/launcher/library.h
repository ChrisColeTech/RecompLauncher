// nxrs - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
inline constexpr auto executableSuffix = ".exe";
inline constexpr auto moduleSuffix = "_code.dll";
inline constexpr auto nativeTarget = "win64";
inline constexpr auto nativeBackend = "win32";
#else
inline constexpr auto executableSuffix = "";
inline constexpr auto moduleSuffix = "_code.so";
inline constexpr auto nativeTarget = "linux";
inline constexpr auto nativeBackend = "x11";
#endif

// The engine a game is built with: Turbo (switchrecomp.py, run through Python) or the nxrs engine (nxrs-build, run
// directly; docs/LAUNCHER.md "Engines"). Entries from before the choice existed are Turbo's.
inline constexpr auto engineTurbo = "turbo";
inline constexpr auto engineNxrs = "nxrs";
inline constexpr auto nxrsBuildName = "nxrs-build";
inline constexpr auto nxrsFolderSuffix = " [nxrs]"; // an nxrs build of a game sits beside Turbo's: "<build folder> [nxrs]"
// The contract nxrs-build has to answer (`nxrs-build contract` prints "nxrs-contract <n>"): 2 = subcommands + @ lines.
inline constexpr int nxrsContractNeeded = 2;

// A game update the user installed for a game (Game > Install Updates…): a package of its own, read in place.
struct GameUpdate {
    QString path;       // the package (absolute)
    QString titleId;    // the game it updates, 16 hex digits
    qint64 version = 0; // content metadata version (262144); 0 when the package has none (a bare NCA)
    QString display;    // "1.1.1", from the update's control data; may be empty
    QString label() const; // "1.1.1 (v262144)"
};

struct Game {
    QString id, title, source, buildDir, outputName, cover, background;
    QString renderer = "vulkan";
    QString engine = engineTurbo;
    bool nxrs() const { return engine == engineNxrs; }
    QString buttonLayout; // Empty inherits the launcher default (Settings > Controller).
    // Screen mode: empty inherits the launcher default (Settings > Play), "tv" (docked=1) or "handheld" (docked=0).
    QString screenMode;
    QString builtSource;
    QStringList dlc; // Add-on content packages (absolute paths) installed with Game > Install DLC….
    // Updates. An update replaces the game's code, so it is applied by building the game again; its data is then read
    // in place from the game's package and the update's. One update applies at a time.
    QVector<GameUpdate> updates;
    QString updateChoice; // "" = the newest installed update, "none" = the game without updates, else a package path
    QString builtUpdate;  // updateToken() of the last build
    const GameUpdate* activeUpdate() const; // the update the next build applies, or nullptr
    QString updateToken() const;            // what the next build applies: "" (the source as it is), "none" or the update's path
    bool updatePending() const;             // built, but with another update than the one chosen now: build again
    bool buildIncomplete = false;
    int workers = 0; // Zero inherits the launcher default.
    int optimization = 2;
    QDateTime lastPlayed, lastBuilt;
    // Execution hints: how many entries <data>/discovered.tsv held when the last build started (that build took them in)
    int hintsBuilt = 0;
    // The engine whose saves were offered to this build once (copy saves between Turbo and nxrs): not offered again.
    QString savesOffered;
    QString executable() const;
    QString codeModule() const;
    QString dataDir() const;
    QString iniPath() const;
    QString format() const;
    QString version() const; // The built game's display version ("1.1.1"), from the control data the build staged.
    bool hasBuild() const;
    bool sourceAvailable() const;
    QJsonObject json() const;
    static Game fromJson(const QJsonObject& object);
};

struct Toolchain {
    QString root, python, compiler, runtime, dotnet, keys;
    // the nxrs engine: nxrs-build and what it says about itself (engine.json beside it, else `nxrs-build contract`)
    QString nxrsBuild, nxrsVersion;
    int nxrsContract = 0;
    bool canBuild() const;   // Turbo
    QString problem() const; // Turbo
    QString nxrsProblem() const;
    bool canBuild(const QString& engine) const { return problemFor(engine).isEmpty(); }
    QString problemFor(const QString& engine) const { return engine == engineNxrs ? nxrsProblem() : problem(); }
    // nxrs-build: NXRS_BUILD, else the one chosen in Settings (a file or its folder), else the release's nxrs-engine/
    // beside the launcher; then reads its contract and version.
    void findNxrs(const QString& chosen);
    QProcessEnvironment environment() const;
    static Toolchain find(const QString& root);
    static QString discoverRoot();
};

// Settings > Play: the player's settings, one value for every game (never per game), written into every game's INI.
struct PlaySettings {
    bool pause = false;   // pause_on_focus_loss: pause while the window is in the background
    bool mute = false;    // mute_on_focus_loss: mute while the window is in the background
    bool docked = true;   // docked: TV mode (sharper) or handheld (lighter); TV by default
    bool askUser = false; // ask_user: ask who is playing when several profiles exist
};
class Library {
public:
    explicit Library(QString file);
    bool load(QString* error);
    bool save(QString* error) const;
    Game* find(const QString& id);
    const Game* find(const QString& id) const;
    bool add(Game game);
    QString importBuild(const Game& game);
    int discoverBuilds(const QString& folder);
    QString path;
    QString toolRoot, outputRoot, keysPath;
    QString nxrsBuild;             // nxrs-build chosen in Settings (or --nxrs-build); empty = the bundled one
    bool toolRootChosen = false;   // picked in Settings; otherwise the engine beside this launcher is used and toolRoot follows it
    int jobs = 8;
    int viewMode = 0, coverWidth = 180;
    bool showBackground = true;
    QString colorMode = "dark", accentColor = "lime";
    QString buttonLayout = "switch"; // Default controller layout written into every game's INI.
    // Settings > Play: the engine's pause_on_focus_loss, one value for every game (off: a game keeps running while its
    // window is in the background, as a PC game does). Written into every game's INI at build, at Play and on change.
    bool pauseOnFocusLoss = false;
    // Settings > Play, beside it: the engine's mute_on_focus_loss (off: the sound goes on in the background).
    bool muteOnFocusLoss = false;
    // Settings > Play, the screen mode: TV mode (docked=1, sharper) by default, handheld (docked=0, lighter)
    bool docked = true;
    bool askUser = false;   // Settings > Play: ask_user (off: the first profile plays)
    PlaySettings play() const;
    PlaySettings playFor(const Game& game) const;   // play() with the game's own screen mode, when it has one
    QByteArray geometry, dockState;
    QVector<Game> games;
private:
    bool writable = true;
};

QString outputNameFor(const QString& title);
QString titleForSource(const QString& source);
bool validSource(const QString& source, QString* error);
Game gameFromSource(const QString& source, const QString& outputRoot = {});
bool gameFromExecutable(const QString& path, Game* game, QString* error);
bool writeRenderer(const QString& ini, const QString& renderer, QString* error);
// The engine's button_layout key: "switch" (by position, the engine default) or "xbox" (by Xbox label).
bool validButtonLayout(const QString& layout);
// "switch"/"xbox" for those values and their engine aliases (nintendo, position(s), label(s)); empty otherwise.
QString normalizeButtonLayout(const QString& layout);
QString effectiveButtonLayout(const Game& game, const QString& launcherDefault);
bool writeButtonLayout(const QString& ini, const QString& layout, QString* error);
// Everything the launcher owns in a game's INI (renderer + button layout), written at build, play and settings change.
bool writeGameSettings(const Game& game, const QString& launcherLayout, const PlaySettings& play, QString* error);
bool writeMuteOnFocusLoss(const QString& ini, bool mute, QString* error);   // [Controls] mute_on_focus_loss=0 / 1
bool writePlaySettings(const QString& ini, const PlaySettings& play, QString* error);   // pause, mute, docked, ask_user
bool readAskUser(const QString& ini);
// "tv" / "handheld" for those values (and their docked / console spellings); empty otherwise.
QString normalizeScreenMode(const QString& mode);
bool effectiveDocked(const Game& game, bool launcherDocked);
// Execution hints (nxrs): a run that interpreted code the build missed appends it to <data>/discovered.tsv (one entry a
// line, '#' lines are comments); the next build translates them. Entries in the file, and those since the last build.
int discoveredCount(const QString& dataDir);
int newDiscoveries(const Game& game);
bool readMuteOnFocusLoss(const QString& ini);
// pause_on_focus_loss (Settings > Play, global): written under [Controls] (the engine's section) as 0 / 1.
bool writePauseOnFocusLoss(const QString& ini, bool pause, QString* error);
bool readPauseOnFocusLoss(const QString& ini);   // false when the INI has no value (the engine's default)
QString readRenderer(const QString& ini);
// The engine's docked key: true = TV mode (the game renders for a television: sharper), false = handheld mode. TV mode when the INI has no value.
bool readDocked(const QString& ini);
bool writeDocked(const QString& ini, bool docked, QString* error);
bool samePath(const QString& left, const QString& right);
// A package that holds the game together with its DLC (1G+1U+1D): the first build from it puts it on the game's DLC list.
// builtBefore is the source of the previous build; a rebuild does not put back a package the user removed from the list.
bool adoptPackageDlc(Game& game, const QString& builtBefore);
// The engine choice. Switching moves the game to that engine's build folder ("<folder> [nxrs]" for nxrs) so both
// builds can exist; what is built there is read back from its data folder. Returns true when something changed.
bool setGameEngine(Game& game, const QString& engine);
QString buildFolderFor(const QString& buildDir, const QString& engine);
// A build on disk made by nxrs-build (its generated C carries nxrs.gen.key); Turbo's otherwise.
bool builtWithNxrs(const QString& buildDir, const QString& outputName);
// The nxrs-build arguments (contract v2) for a build, an update check and a DLC sync.
QStringList nxrsBuildArguments(const Game& game, const QString& keys, int jobs);
QStringList nxrsUpdateInfoArguments(const Game& game, const QString& keys, const QStringList& packages);
QStringList nxrsDlcSyncArguments(const Game& game, const QString& keys);

// `nxrs-build doctor`: what nxrs-build checks on this machine (CPU, toolchain, compiler, engine libraries, Vulkan
// devices, free disk, keys), one "ok   <check>" / "FAIL <check>" line each; other lines are notes. Exit 0 = all ok.
struct DoctorCheck {
    bool ok = false;
    QString text;
};
struct NxrsDoctor {
    QVector<DoctorCheck> checks;
    QStringList notes;
    int exitCode = -1;
    QString error;   // nxrs-build did not run or did not finish
    bool ok() const { return error.isEmpty() && exitCode == 0 && !checks.isEmpty(); }
    int failed() const;
    QString text() const;   // one line per check, "✓"/"✗" first, notes as they came
};
inline QStringList nxrsDoctorArguments() { return {"doctor"}; }
NxrsDoctor parseNxrsDoctor(const QString& output, int exitCode);
// Runs nxrs-build doctor and waits (Settings runs the same arguments without waiting; tests and --nxrs-doctor wait).
NxrsDoctor runNxrsDoctor(const QString& nxrsBuild, const QProcessEnvironment& environment, int timeoutMs = 60000);

// Saves of the two builds of one game (Turbo's in <build folder>, nxrs's in "<build folder> [nxrs]"): both keep them in
// <name>_data\save\. Turbo (runtime/svc_fs.c) has one folder per save type: user (every account's saves), device,
// cache, bcat, temp; its runtimes before the split kept everything in system. nxrs (os services/fs.rs) has one folder
// per account (user1 is the first), device, cache<n>, temp, system\<id>, and makes user<n> / device transactional
// (<folder>.work / .new / .old beside them while a game runs). What is copied: user <-> user1 (Turbo's old system
// when it has no user), device <-> device, cache <-> cache0; temporary data and BCAT are not.
struct SaveFolderPair {
    QString from, to;   // absolute
};
QString saveFolder(const QString& buildDir, const QString& outputName);   // <buildDir>/<outputName>_data/save
QVector<SaveFolderPair> saveCopyPlan(const QString& fromSave, const QString& fromEngine, const QString& toSave);
// The other engine's build of this game holds saves this one can take.
bool otherBuildHasSaves(const Game& game);
// Copies the other engine's saves of `game` into the build of its current engine. The destination's save folder is
// backed up first (copied whole to save-backup-<time> beside it; `backup` gets its path, empty when there was nothing
// to back up). `report` lists what was copied. The source is only read.
bool copySavesFromOtherEngine(const Game& game, QString* backup, QString* report, QString* error);

// Add-on content (DLC). The engine installs a package into the built game's data folder (switchrecomp.py
// --install-dlc / --sync-dlc: <data>/dlc.manifest plus <data>/dlc/<title id>/); the launcher keeps the list of
// packages in Game::dlc and has the engine bring the data folder in line after a build and before Play.
inline constexpr auto updateFileFilter = "Game updates (*.nsp *.xci *.nca);;All files (*)";
// The build on disk was made with an update of its own package (its romfs.nsp names it) but the library does not
// say so (an imported build, or a library file written since by a launcher from before updates): the update joins
// the list and counts as built. Returns true when the game changed.
bool adoptBuiltUpdate(Game& game);
// A quick look at the file itself before the engine is asked: a type an update can be read from in place.
bool validUpdatePackage(const QString& path, QString* error);
// The engine arguments that say what the update packages are and check them against the game's source.
QStringList updateInfoArguments(const Game& game, const QString& toolRoot, const QStringList& packages);
// One line of the engine's answer: "UPDATE\t<path>\t<title id>\t<version>\t<display>" fills update and returns
// true; "UPDATE error: <reason>" fills error and returns false; any other line returns false with error empty.
bool parseUpdateInfo(const QString& line, GameUpdate* update, QString* error);
// "Update [v262144].nsp — 1.1.1 (v262144) — applied" for the lists.
QString updateDescription(const Game& game, const GameUpdate& update);
// "1.1.1 (v262144)", "the game without updates", for status lines.
QString updateTokenLabel(const Game& game, const QString& token);

inline constexpr auto dlcFileFilter = "Add-on content (*.nsp *.nsz *.xci *.xcz *.nca);;All files (*)";
struct DlcTitle {
    QString titleId; // 16 hex digits
    qint64 version = 0;
    bool stub = false; // installed without its data (diagnostics only)
    QString source;    // the package it was installed from
};
// The titles installed in a built game's data folder (the lines of dlc.manifest); empty when there is none.
QVector<DlcTitle> readDlcManifest(const QString& dataDir);
// A quick look at the file itself: one of the package types the engine installs, with the right signature.
bool validDlcPackage(const QString& path, QString* error);
// True when every package in game.dlc is installed in the data folder and nothing else is.
bool dlcUpToDate(const Game& game);
// Adds packages that are installed in the data folder but missing from game.dlc (installed from the command
// line, or by a launcher whose library file was since overwritten). Returns true when the list changed.
bool adoptInstalledDlc(Game& game);
// The engine arguments (after the Python interpreter) that make the data folder match game.dlc.
QStringList dlcSyncArguments(const Game& game, const QString& toolRoot);
// "Booster Course.nsp — 0100152000023001 v65536" for the Properties list.
QString dlcDescription(const QString& package, const QVector<DlcTitle>& installed, bool built);
bool inPlacePackage(const QString& path);
QString toolkitProjectRoot(const QString& toolRoot);
QStringList switchrSearchRoots(const QString& toolRoot);
bool isReleaseEngine(const QString& toolRoot);
// The engine beside this launcher (a release's engine/, a checkout's submodule) wins over the remembered toolRoot unless
// an engine was picked in Settings and still exists: a fresh install must not keep using an older install's engine.
void resolveToolRoot(Library& library, const QString& discovered);
QString recompCli(const QString& toolRoot);

struct LibraryFilter {
    QString text;
    int state = 0; // All, Ready, Needs build, Running.
    int source = 0; // All, package, NRO, extracted, existing native build.
    bool matches(const Game& game, bool running = false, bool building = false) const;
    int activeCount() const;
};

struct CardState {
    QString action, status;
    bool actionEnabled = false, needsAttention = false, building = false, running = false;
};
CardState cardState(const Game& game, bool running, bool building, bool otherBuild, bool toolsReady);
