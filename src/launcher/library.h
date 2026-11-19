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
    QString buttonLayout; // Empty inherits the launcher default (Settings > Controller).
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
    bool canBuild() const;
    QString problem() const;
    QProcessEnvironment environment() const;
    static Toolchain find(const QString& root);
    static QString discoverRoot();
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
    bool toolRootChosen = false;   // picked in Settings; otherwise the engine beside this launcher is used and toolRoot follows it
    int jobs = 8;
    int viewMode = 0, coverWidth = 180;
    bool showBackground = true;
    QString colorMode = "dark", accentColor = "lime";
    QString buttonLayout = "switch"; // Default controller layout written into every game's INI.
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
bool writeGameSettings(const Game& game, const QString& launcherLayout, QString* error);
QString readRenderer(const QString& ini);
// The engine's docked key: true = TV mode (the game renders for a television: sharper), false = handheld mode. TV mode when the INI has no value.
bool readDocked(const QString& ini);
bool writeDocked(const QString& ini, bool docked, QString* error);
bool samePath(const QString& left, const QString& right);
// A package that holds the game together with its DLC (1G+1U+1D): the first build from it puts it on the game's DLC list.
// builtBefore is the source of the previous build; a rebuild does not put back a package the user removed from the list.
bool adoptPackageDlc(Game& game, const QString& builtBefore);

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
