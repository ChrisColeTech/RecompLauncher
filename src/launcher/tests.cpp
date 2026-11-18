// RecompLauncher - native game library and build launcher.
// The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
#include "buildmanager.h"
#include "theme.h"
#include "processwatch.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class LauncherTests : public QObject {
    Q_OBJECT
private slots:
    void pythonPrecedenceAndEnvironment() {
        const auto oldPython=qgetenv("SWR_PYTHON"),oldPath=qgetenv("PATH");
        const bool hadPython=qEnvironmentVariableIsSet("SWR_PYTHON");
        const auto restore=qScopeGuard([&]{if(hadPython)qputenv("SWR_PYTHON",oldPython);else qunsetenv("SWR_PYTHON");qputenv("PATH",oldPath);});
        qunsetenv("SWR_PYTHON");
        QTemporaryDir tmp(QDir::tempPath()+"/swr python spaces-XXXXXX");QVERIFY(tmp.isValid());
#ifdef Q_OS_WIN
        const QString suffix="Scripts/python.exe",bundle="python/python.exe",fallback="path/python.exe";
#else
        const QString suffix="bin/python3",bundle="python/bin/python3",fallback="path/python3";
#endif
        const auto stage=[&](const QString& relative){const auto path=tmp.filePath(relative);QDir().mkpath(QFileInfo(path).absolutePath());return QFile::copy(QCoreApplication::applicationFilePath(),path);};
        QVERIFY(stage(fallback));QVERIFY(stage(bundle));QVERIFY(stage("venv/"+suffix));QVERIFY(stage(".venv/"+suffix));
        qputenv("PATH",(QDir::toNativeSeparators(tmp.filePath("path"))+QDir::listSeparator()).toUtf8()+oldPath);
        auto tools=Toolchain::find(tmp.path());QCOMPARE(tools.python,tmp.filePath(".venv/"+suffix));
        QCOMPARE(tools.environment().value("SWR_PYTHON"),tools.python);
        qputenv("SWR_PYTHON",tmp.filePath(bundle).toUtf8());QCOMPARE(Toolchain::find(tmp.path()).python,tmp.filePath(bundle));
        qputenv("SWR_PYTHON",tmp.filePath("missing.exe").toUtf8());QVERIFY(Toolchain::find(tmp.path()).python.isEmpty());
        qunsetenv("SWR_PYTHON");QVERIFY(QFile::remove(tmp.filePath(".venv/"+suffix)));
        QCOMPARE(Toolchain::find(tmp.path()).python,tmp.filePath("venv/"+suffix));
        QVERIFY(QFile::remove(tmp.filePath("venv/"+suffix)));QCOMPARE(Toolchain::find(tmp.path()).python,tmp.filePath(bundle));
        QVERIFY(QFile::remove(tmp.filePath(bundle)));QCOMPARE(Toolchain::find(tmp.path()).python,tmp.filePath(fallback));
    }
    void releaseAndCheckoutLayouts() {
        const auto oldPython=qgetenv("SWR_PYTHON");const bool hadPython=qEnvironmentVariableIsSet("SWR_PYTHON");
        const auto restore=qScopeGuard([&]{if(hadPython)qputenv("SWR_PYTHON",oldPython);else qunsetenv("SWR_PYTHON");});
        qunsetenv("SWR_PYTHON");
#ifdef Q_OS_WIN
        const QString python="python/python.exe",venv=".venv/Scripts/python.exe",gcc="gcc/Library/bin/x86_64-w64-mingw32-gcc.exe",
            runtime="bin/runtime_win64_win32.exe",dotnet="dotnet/dotnet.exe",shaders="shader-tools/Library/bin";
#else
        const QString python="python/bin/python3",venv=".venv/bin/python3",gcc="gcc/bin/x86_64-conda-linux-gnu-gcc",
            runtime="bin/runtime_linux_x11",dotnet="dotnet/dotnet",shaders="shader-tools/bin";
#endif
        const auto stage=[](const QDir& dir,const QString& relative){const auto path=dir.filePath(relative);QDir().mkpath(QFileInfo(path).absolutePath());return QFile::copy(QCoreApplication::applicationFilePath(),path);};
        // release: <top>/engine + <top>/toolchain; the CLI ships inside engine/
        QTemporaryDir release(QDir::tempPath()+"/swr release spaces-XXXXXX");QVERIFY(release.isValid());const QDir top(release.path());
        for(const auto& name:{QString("engine/switchrecomp.py"),QString("engine/recomp_cli.py"),"engine/"+runtime,"toolchain/"+python,"toolchain/"+gcc,"toolchain/"+dotnet})QVERIFY(stage(top,name));
        QVERIFY(top.mkpath("toolchain/"+shaders));
        const QString engine=top.filePath("engine");QVERIFY(isReleaseEngine(engine));
        QCOMPARE(switchrSearchRoots(engine),QStringList({engine,top.filePath("toolchain"),top.absolutePath()}));
        auto tools=Toolchain::find(engine);
        QCOMPARE(tools.python,top.filePath("toolchain/"+python));QCOMPARE(tools.compiler,top.filePath("toolchain/"+gcc));
        QCOMPARE(tools.runtime,top.filePath("engine/"+runtime));QCOMPARE(tools.dotnet,top.filePath("toolchain/"+dotnet));
        QVERIFY2(tools.canBuild(),qPrintable(tools.problem()));
        QCOMPARE(BuildManager::arguments(Game{},tools,1)[1],top.filePath("engine/recomp_cli.py"));
        const auto env=tools.environment();QCOMPARE(QDir(env.value("NXSHADER_TOOLS")),QDir(top.filePath("toolchain/"+shaders)));
        QVERIFY(env.value("PATH").split(QDir::listSeparator()).contains(top.absolutePath()));
        // checkout: <project>/third_party/Switch2RECOMP, .venv and third_party/gcc at <project>, CLI in src/
        QTemporaryDir checkout(QDir::tempPath()+"/swr checkout spaces-XXXXXX");QVERIFY(checkout.isValid());const QDir project(checkout.path());
        for(const auto& name:{QString("third_party/Switch2RECOMP/switchrecomp.py"),"third_party/Switch2RECOMP/"+runtime,venv,"third_party/"+gcc})QVERIFY(stage(project,name));
        const QString vendored=project.filePath("third_party/Switch2RECOMP");QVERIFY(!isReleaseEngine(vendored));
        tools=Toolchain::find(vendored);
        QCOMPARE(tools.python,project.filePath(venv));QCOMPARE(tools.compiler,project.filePath("third_party/"+gcc));
        QCOMPARE(tools.runtime,project.filePath("third_party/Switch2RECOMP/"+runtime));
        QCOMPARE(BuildManager::arguments(Game{},tools,1)[1],project.filePath("src/recomp_cli.py"));
        QCOMPARE(toolkitProjectRoot(vendored),project.absolutePath());
    }
    void discoverNspBuildAndPreserveLibrary() {
        QTemporaryDir tmp;const auto build=tmp.filePath("MKW NSP build");QVERIFY(QDir().mkpath(build+"/Mario Kart World 180_data"));
        const auto source=tmp.filePath("Mario Kart World 1.8.0.nsp");
        for(const auto& path:{source,build+"/Mario Kart World 180"+executableSuffix,build+"/Mario Kart World 180"+moduleSuffix}){
            QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("fixture");
        }
        {QFile marker(build+"/Mario Kart World 180_data/romfs.nsp");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("SWRNSP1\n"+QDir::toNativeSeparators(source).toUtf8()+"\n100\n200\n");}
        {QFile ini(build+"/Mario Kart World 180.ini");QVERIFY(ini.open(QIODevice::WriteOnly));ini.write("[Graphics]\nrenderer=vulkan\n");}
        Library library(tmp.filePath("library.json"));library.jobs=3;library.colorMode="light";
        auto old=gameFromSource(tmp.filePath("Older game.nup"));old.cover="my cover.png";QVERIFY(library.add(old));
        QCOMPARE(library.discoverBuilds(tmp.path()),1);QCOMPARE(library.games.size(),2);
        const auto game=library.games.back();QVERIFY(game.hasBuild());QCOMPARE(game.format(),QString("DIRECT NSP"));
        QCOMPARE(game.renderer,QString("vulkan"));QVERIFY(samePath(game.source,source));QVERIFY(game.sourceAvailable());
        QVERIFY(cardState(game,false,false,false,false).actionEnabled);
        QCOMPARE(library.discoverBuilds(tmp.path()),0);QCOMPARE(library.importBuild(game),game.id);
        QCOMPARE(library.games.front().json(),old.json());QCOMPARE(library.jobs,3);QCOMPARE(library.colorMode,QString("light"));
        Library pending(tmp.filePath("pending.json"));auto unbuilt=gameFromSource(source);unbuilt.cover="chosen.png";unbuilt.title="My MKW";unbuilt.workers=5;pending.add(unbuilt);
        QCOMPARE(pending.importBuild(game),unbuilt.id);QCOMPARE(pending.games.size(),1);QVERIFY(pending.games.front().hasBuild());
        QCOMPARE(pending.games.front().cover,unbuilt.cover);QCOMPARE(pending.games.front().title,unbuilt.title);QCOMPARE(pending.games.front().workers,5);
        QVERIFY(QFile::remove(source));QCOMPARE(cardState(library.games.back(),false,false,false,true).status,QString("Source offline"));
        QVERIFY(QFile::remove(game.codeModule()));QVERIFY(!library.games.back().hasBuild());
    }
    void themeTextContrast() {
        const auto luminance=[](const QColor& color){
            const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
            return .2126*linear(color.redF())+.7152*linear(color.greenF())+.0722*linear(color.blueF());
        };
        const auto contrast=[&](QColor a,QColor b){double x=luminance(a),y=luminance(b);return (qMax(x,y)+.05)/(qMin(x,y)+.05);};
        for(const auto& mode:{"light","dark"})for(const auto& accent:Theme::accents()){
            const auto colors=Theme::colorsFor(mode,accent.id);
            for(const auto& background:{colors.canvas,colors.surface,colors.raised,colors.selected}) {
                QVERIFY(contrast(colors.text,background)>=4.5);
                QVERIFY(contrast(colors.muted,background)>=4.5);
            }
            QVERIFY(contrast(colors.accentText,colors.accent)>=4.5);
        }
    }
    void searchAndFiltersCombine() {
        Game game;game.title="Donkey Kong Bananza";game.source="P:/Bananza.nup";
        LibraryFilter filter{"BANAN",2,1};QVERIFY(filter.matches(game));QCOMPARE(filter.activeCount(),3);
        filter.source=2;QVERIFY(!filter.matches(game));filter.source=1;
        filter.state=3;QVERIFY(!filter.matches(game));QVERIFY(filter.matches(game,true));
        filter.state=2;QVERIFY(!filter.matches(game,false,true));
        filter.state=1;QVERIFY(!filter.matches(game));
        filter={};QVERIFY(filter.matches(game));QCOMPARE(filter.activeCount(),0);
    }
    void rendererPreservesSettings() {
        QTemporaryDir tmp; const QString path=tmp.filePath("Game.ini");
        const QByteArray original="; keep this comment\r\n[Graphics]\r\nrenderer=opengl\r\nresolution=0.75\r\n[Audio]\r\naudio_slowdown=0\r\n";
        {QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(original);}
        QString error; QVERIFY2(writeRenderer(path,"vulkan",&error),qPrintable(error));
        QFile result(path); QVERIFY(result.open(QIODevice::ReadOnly)); const auto text=result.readAll();
        QVERIFY(text.contains("renderer=vulkan\r\n")); QVERIFY(text.contains("resolution=0.75"));
        QVERIFY(text.contains("; keep this comment")); QVERIFY(text.contains("audio_slowdown=0"));
    }
    void screenModeIni() {
        QTemporaryDir tmp; const QString path=tmp.filePath("Game.ini");
        QVERIFY(readDocked(path));   // no INI yet: TV mode, the engine's default
        {QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("[Display]\r\ndocked=1\r\nfullscreen=1\r\n[Graphics]\r\nrenderer=vulkan\r\n");}
        QString error; QVERIFY2(writeDocked(path,false,&error),qPrintable(error)); QVERIFY(!readDocked(path));
        {QFile result(path); QVERIFY(result.open(QIODevice::ReadOnly)); const auto text=result.readAll();
         QVERIFY(text.contains("docked=0\r\n")); QVERIFY(text.contains("fullscreen=1")); QVERIFY(text.contains("renderer=vulkan")); QVERIFY(text.contains("settings_version=2"));}
        QVERIFY2(writeDocked(path,true,&error),qPrintable(error)); QVERIFY(readDocked(path));
    }
    void buttonLayoutIni() {
        QTemporaryDir tmp; const QString path=tmp.filePath("Game.ini");
        // the runtime's own INI: [Controls] already has the key (it writes every key with its default)
        const QByteArray original="; keep\r\n[Controls]\r\n; help text\r\nbutton_layout=switch\r\nbutton_map=south:a\r\n[Graphics]\r\nrenderer=opengl\r\n";
        {QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(original);}
        QString error; QVERIFY2(writeButtonLayout(path,"xbox",&error),qPrintable(error));
        auto read=[&path]{QFile f(path); return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();};
        QByteArray expected=original; expected.replace("button_layout=switch","button_layout=xbox"); QCOMPARE(read(),expected);
        QVERIFY(!writeButtonLayout(path,"sega",&error)); QVERIFY(!writeButtonLayout(path,"",&error)); QCOMPARE(read(),expected);
        // an older INI without the key: added under [Controls], or a new [Controls] section
        {QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate)); f.write("[Controls]\nmouse_invert=0\n");}
        QVERIFY(writeButtonLayout(path,"switch",&error)); QCOMPARE(read(),QByteArray("[Controls]\nbutton_layout=switch\nmouse_invert=0\n"));
        QVERIFY(QFile::remove(path)); QVERIFY(writeButtonLayout(path,"xbox",&error)); QVERIFY(read().contains("[Controls]\nbutton_layout=xbox\n"));
        // inherit vs override, and the full set the launcher writes at build/play time
        Game game; game.buildDir=tmp.path(); game.outputName="Game"; game.renderer="vulkan";
        QCOMPARE(effectiveButtonLayout(game,"xbox"),QString("xbox")); QCOMPARE(effectiveButtonLayout(game,"switch"),QString("switch"));
        QCOMPARE(effectiveButtonLayout(game,"bogus"),QString("switch"));
        game.buttonLayout="switch"; QCOMPARE(effectiveButtonLayout(game,"xbox"),QString("switch"));
        QVERIFY(QFile::remove(path)); QVERIFY2(writeGameSettings(game,"xbox",&error),qPrintable(error));
        QVERIFY(read().contains("renderer=vulkan")); QVERIFY(read().contains("button_layout=switch"));
        game.buttonLayout.clear(); QVERIFY(writeGameSettings(game,"xbox",&error)); QVERIFY(read().contains("button_layout=xbox")); QVERIFY(!read().contains("button_layout=switch"));
        // what the game itself wrote into its INI (F11: fullscreen=0) is not Play's to reset
        {QFile f(path); QVERIFY(f.open(QIODevice::Append)); f.write("\r\n[Display]\r\nfullscreen=0\r\n");}
        QVERIFY(writeGameSettings(game,"xbox",&error)); QVERIFY(read().contains("fullscreen=0")); QVERIFY(!read().contains("fullscreen=1"));
        QVERIFY(validButtonLayout("") && validButtonLayout("xbox") && !validButtonLayout("label") && !validButtonLayout("nintendo"));
        // the engine's old/alias names are read but never written
        QCOMPARE(normalizeButtonLayout("nintendo"),QString("switch")); QCOMPARE(normalizeButtonLayout("labels"),QString("xbox"));
        game.buttonLayout="nintendo"; QCOMPARE(effectiveButtonLayout(game,"xbox"),QString("switch")); QVERIFY(!writeButtonLayout(path,"nintendo",&error));
        QCOMPARE(Game::fromJson({{"buttonLayout","nintendo"}}).buttonLayout,QString("switch"));
    }
    void libraryRoundTripAndCorruption() {
        QTemporaryDir tmp; const QString file=tmp.filePath("nested/library.json");
        Library lib(file); Game game=gameFromSource(tmp.filePath("My Game [v1].nup"));
        game.cover="art.png"; game.background="wallpaper.png"; game.renderer="vulkan"; game.workers=3; game.optimization=1; game.buttonLayout="switch";
        lib.add(game); lib.buttonLayout="xbox"; lib.jobs=4; lib.viewMode=1; lib.coverWidth=220; lib.showBackground=false;lib.colorMode="light";lib.accentColor="violet";lib.keysPath="D:/Keys/prod.keys";
        QString error; QVERIFY2(lib.save(&error),qPrintable(error));
        Library loaded(file); QVERIFY(loaded.load(&error)); QCOMPARE(loaded.games.size(),1); QCOMPARE(loaded.jobs,4);
        QCOMPARE(loaded.games.front().source,game.source); QCOMPARE(loaded.games.front().title,QString("My Game"));
        QCOMPARE(loaded.games.front().background,QString("wallpaper.png")); QCOMPARE(loaded.games.front().workers,3);
        QCOMPARE(loaded.viewMode,1); QCOMPARE(loaded.coverWidth,220); QVERIFY(!loaded.showBackground);
        QCOMPARE(loaded.colorMode,QString("light"));QCOMPARE(loaded.accentColor,QString("violet"));
        QCOMPARE(loaded.keysPath,QString("D:/Keys/prod.keys"));
        QCOMPARE(loaded.buttonLayout,QString("xbox")); QCOMPARE(loaded.games.front().buttonLayout,QString("switch"));
        // a new library defaults to the engine default; games without the key inherit; unknown values never reach an INI
        QCOMPARE(Library(file).buttonLayout,QString("switch")); QVERIFY(Game::fromJson({{"buttonLayout","sega"}}).buttonLayout.isEmpty());
        QVERIFY(Game::fromJson({}).buttonLayout.isEmpty());
        QVERIFY(!loaded.add(game));
        {QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate)); f.write("bad json");}
        Library broken(file); QVERIFY(!broken.load(&error)); QVERIFY(!broken.save(&error));
        QFile f(file); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(),QByteArray("bad json"));
    }
    void argumentsPreserveLiteralPaths() {
        Game game; game.source="P:/Games/Name [v1] & friends.nup"; game.buildDir="P:/Build [x]"; game.outputName="Name";
        Toolchain t; t.root="D:/Toolkit path"; t.compiler="D:/GCC path/gcc.exe"; t.runtime="D:/Toolkit path/bin/runtime.exe";
        const auto args=BuildManager::arguments(game,t,8);
        QVERIFY(args.contains(game.source)); QVERIFY(args.contains(game.buildDir)); QVERIFY(args.contains(t.compiler));
        QCOMPARE(args[args.indexOf("--jobs")+1],QString("8"));
        game.workers=3;game.optimization=1;const auto perGame=BuildManager::arguments(game,t,8);
        QCOMPARE(perGame[perGame.indexOf("--jobs")+1],QString("3"));
        QCOMPARE(perGame[perGame.indexOf("-O")+1],QString("1"));
    }
    // A built game on disk: executable, code module, data folder with exefs/, source marked as built.
    static Game builtGame(const QTemporaryDir& tmp) {
        Game game; game.id="g"; game.title="Kart"; game.outputName="Kart"; game.buildDir=tmp.filePath("Build output");
        QDir().mkpath(game.dataDir()+"/exefs");
        for(const auto& path:{game.executable(),game.codeModule()}){QFile f(path);f.open(QIODevice::WriteOnly);f.write("fixture");}
        return game;
    }
    static void writeFile(const QString& path,const QByteArray& bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write(bytes);}
    void dlcListRoundTrip() {
        QTemporaryDir tmp; const QString file=tmp.filePath("library.json");
        Library lib(file); Game game=gameFromSource(tmp.filePath("Kart [v0].nsp"));
        game.dlc={"P:/Games/Kart/Booster [0100152000023001][v65536].nsp","P:/Games/Kart/Kart (1G+1U+1D).nsp"};
        lib.add(game); QString error; QVERIFY2(lib.save(&error),qPrintable(error));
        Library loaded(file); QVERIFY(loaded.load(&error)); QCOMPARE(loaded.games.front().dlc,game.dlc);
        // a library written before DLC existed loads with none; the same package twice is one entry; junk is dropped
        QVERIFY(Game::fromJson({{"id","x"},{"title","Old"}}).dlc.isEmpty());
        const auto twice=Game::fromJson({{"dlc",QJsonArray{"P:/a/dlc.nsp","P:\\a\\DLC.nsp","",7,"P:/a/other.nsz"}}});
#ifdef Q_OS_WIN
        QCOMPARE(twice.dlc,QStringList({"P:/a/dlc.nsp","P:/a/other.nsz"}));
#else
        QCOMPARE(twice.dlc.size(),3);
#endif
        QVERIFY(Game().json().contains("dlc"));
    }
    void dlcManifestAndState() {
        QTemporaryDir tmp; Game game=builtGame(tmp); QVERIFY(game.hasBuild());
        const QString booster=tmp.filePath("Booster [0100152000023001][v65536].nsp"),combined=tmp.filePath("Kart (1G+1U+1D).nsp");
        QVERIFY(readDlcManifest(game.dataDir()).isEmpty()); QVERIFY(dlcUpToDate(game));       // no DLC, no manifest
        game.dlc={booster}; QVERIFY(!dlcUpToDate(game));                                         // listed, not installed yet
        QCOMPARE(dlcDescription(booster,{},true),QString("Booster [0100152000023001][v65536].nsp — file not found"));
        writeFile(booster,"PFS0"); writeFile(combined,"PFS0");
        QCOMPARE(dlcDescription(booster,{},true),QString("Booster [0100152000023001][v65536].nsp — not installed yet"));
        QCOMPARE(dlcDescription(booster,{},false),QString("Booster [0100152000023001][v65536].nsp — installs when the game is built"));
        writeFile(game.dataDir()+"/dlc.manifest",("# SwitchRecomp add-on content: <title id> <version> <data|stub> <package>\r\n"
            "0100152000023001 65536 data "+QDir::toNativeSeparators(booster)+"\r\n"
            "\r\nnot a manifest line\r\n"
            "0100152000023002 3 "+combined+"\n"                                              // the three-column form
            "0100152000023003 0 stub "+combined+"\n").toUtf8());
        const auto titles=readDlcManifest(game.dataDir()); QCOMPARE(titles.size(),3);
        QCOMPARE(titles[0].titleId,QString("0100152000023001")); QCOMPARE(titles[0].version,qint64(65536)); QVERIFY(!titles[0].stub);
        QVERIFY(samePath(titles[0].source,booster)); QVERIFY(samePath(titles[1].source,combined)); QVERIFY(!titles[1].stub);
        QVERIFY(titles[2].stub); QCOMPARE(titles[2].version,qint64(0));
        QCOMPARE(dlcDescription(booster,titles,true),QString("Booster [0100152000023001][v65536].nsp — 0100152000023001 v65536"));
        QCOMPARE(dlcDescription(combined,titles,true),QString("Kart (1G+1U+1D).nsp — 0100152000023002 v3, 0100152000023003 (no data)"));
        QVERIFY(!dlcUpToDate(game));                                                              // the combined package is installed but not listed
        QVERIFY(adoptInstalledDlc(game)); QCOMPARE(game.dlc.size(),2); QVERIFY(samePath(game.dlc[1],combined));
        QVERIFY(!adoptInstalledDlc(game)); QVERIFY(dlcUpToDate(game));
        game.dlc.removeLast(); QVERIFY(!dlcUpToDate(game));                                       // removed in Properties: the data folder is behind
        const auto args=dlcSyncArguments(game,"D:/Engine path");
        QCOMPARE(args.mid(0,3),QStringList({"-u",recompCli("D:/Engine path"),"--sync-dlc"}));
        QCOMPARE(args.mid(3),QStringList({QDir::toNativeSeparators(booster),"--datadir",QDir::toNativeSeparators(game.dataDir())}));
        game.dlc.clear(); QCOMPARE(dlcSyncArguments(game,"D:/Engine path").mid(2),QStringList({"--sync-dlc","--datadir",QDir::toNativeSeparators(game.dataDir())}));
    }
    void dlcPackagesAreCheckedBeforeTheEngineRuns() {
        QTemporaryDir tmp; QString error;
        QByteArray card(0x200,'\0'); card.replace(0x100,4,"HEAD"); QByteArray dumped(0x1200,'\0'); dumped.replace(0x1100,4,"HEAD");
        const QList<QPair<QString,QByteArray>> good{{"a.nsp","PFS0...."},{"b.NSZ","PFS0...."},{"c.xci",card},{"d.xcz",dumped},{"e.nca",QByteArray(0xC00,'x')}};
        for(const auto& item:good){writeFile(tmp.filePath(item.first),item.second);QVERIFY2(validDlcPackage(tmp.filePath(item.first),&error),qPrintable(item.first+": "+error));}
        const QList<QPair<QString,QByteArray>> bad{{"notes.txt","PFS0"},{"game.nup","NUP\1"},{"f.nsp","HEAD"},{"g.xci","PFS0"},{"h.nca","tiny"}};
        for(const auto& item:bad){writeFile(tmp.filePath(item.first),item.second);error.clear();QVERIFY2(!validDlcPackage(tmp.filePath(item.first),&error),qPrintable(item.first));QVERIFY(!error.isEmpty());}
        QVERIFY(!validDlcPackage(tmp.filePath("missing.nsp"),&error)); QVERIFY(!validDlcPackage(tmp.path(),&error));
        for(const auto& suffix:{"nsp","nsz","xci","xcz","nca"})QVERIFY(QString(dlcFileFilter).contains(QString("*.")+suffix));
    }
    void dlcInstallerRunsTheEngineSync() {
        // this test binary stands in for Python + the engine: see main() below
        QTemporaryDir tmp; Game game=builtGame(tmp);
        Toolchain tools; tools.root=tmp.path(); tools.python=QCoreApplication::applicationFilePath(); tools.keys=tmp.filePath("prod.keys");
        const QString good=tmp.filePath("Booster pack.nsp"),refused=tmp.filePath("refused.nsp");
        game.dlc={good,refused};
        DlcInstaller installer; QString error;
        QVERIFY(!installer.start(game,tools,&error)); QVERIFY(error.contains("prod.keys"));     // no keys: say so before starting anything
        writeFile(tools.keys,"header_key = 00");
        QSignalSpy finished(&installer,&DlcInstaller::finished); QStringList lines;
        connect(&installer,&DlcInstaller::output,this,[&lines](const QString& line){lines<<line;});
        QVERIFY2(installer.start(game,tools,&error),qPrintable(error)); QVERIFY(installer.busy());
        QVERIFY(!installer.start(game,tools,&error));                                            // one at a time
        QVERIFY(finished.wait(15000)); QVERIFY(!installer.busy());
        QCOMPARE(finished[0][0].toString(),game.id); QCOMPARE(finished[0][1].toBool(),false);
        QCOMPARE(finished[0][2].toStringList(),QStringList("refused.nsp: the ticket is bound to one console"));
        QVERIFY(lines.contains("DLC: installed 0100152000023001 v65536"));
        const auto titles=readDlcManifest(game.dataDir()); QCOMPARE(titles.size(),1); QVERIFY(samePath(titles[0].source,good));
        game.dlc={good}; QVERIFY(dlcUpToDate(game));
        finished.clear(); QVERIFY(installer.start(game,tools,&error)); QVERIFY(finished.wait(15000));
        QCOMPARE(finished[0][1].toBool(),true); QVERIFY(finished[0][2].toStringList().isEmpty());
        // not built: nothing to install into
        Game unbuilt; unbuilt.buildDir=tmp.filePath("nowhere"); unbuilt.outputName="X"; unbuilt.dlc={good};
        QVERIFY(!installer.start(unbuilt,tools,&error)); QVERIFY(error.contains("Build the game first"));
    }
    void importChecksAllArtifacts() {
        QTemporaryDir tmp; const QString exe=tmp.filePath(QString("My Game")+executableSuffix); QString error; Game g;
        {QFile f(exe); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("test");}
        QVERIFY(!gameFromExecutable(exe,&g,&error));
        {QFile f(tmp.filePath(QString("My Game")+moduleSuffix)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("test");}
        QVERIFY(QDir().mkpath(tmp.filePath("My Game_data")));
        QVERIFY(gameFromExecutable(exe,&g,&error)); QVERIFY(g.hasBuild());
        const auto ready=cardState(g,false,false,true,false);QCOMPARE(ready.action,QString("Play"));QVERIFY(ready.actionEnabled);QVERIFY(!ready.needsAttention);
        g.buildIncomplete=true; QVERIFY(!g.hasBuild()); g.buildIncomplete=false;
        g.source=tmp.filePath("Missing.nup");g.builtSource=QDir::toNativeSeparators(g.source);QVERIFY(g.hasBuild());
        const auto offline=cardState(g,false,false,false,true);QVERIFY(!offline.actionEnabled);QVERIFY(offline.needsAttention);QCOMPARE(offline.status,QString("Source offline"));
        g.source="another.nup"; QVERIFY(!g.hasBuild());
    }
    void perCardActionTargetsCorrectOperation(){
        QTemporaryDir tmp;Game g=gameFromSource(tmp.filePath("Game.nup"));
        {QFile f(g.source);QVERIFY(f.open(QIODevice::WriteOnly));f.write("NUP\1");}
        auto state=cardState(g,false,false,false,true);QCOMPARE(state.action,QString("Build"));QVERIFY(state.needsAttention);QVERIFY(state.actionEnabled);
        QVERIFY(!cardState(g,false,false,true,true).actionEnabled);
        state=cardState(g,true,false,true,true);QCOMPARE(state.action,QString("Stop"));QVERIFY(state.actionEnabled);
        state=cardState(g,false,true,true,true);QCOMPARE(state.action,QString("Cancel build"));QVERIFY(state.actionEnabled);
    }
    void matchesRunningExecutableByFullPath(){
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        Game self;self.id="self";QFileInfo executable(QCoreApplication::applicationFilePath());self.outputName=executable.completeBaseName();self.buildDir=executable.absolutePath();
        QTemporaryDir tmp;Game different=self;different.id="different";different.buildDir=tmp.path();
        const auto running=findRunningGames({self,different});QCOMPARE(running.value(self.id),QCoreApplication::applicationPid());QVERIFY(!running.contains(different.id));
        QVERIFY(!requestGameClose(QCoreApplication::applicationPid(),different.executable()));
#endif
    }
    void pathFormattingDoesNotInvalidateBuild(){
#ifdef Q_OS_WIN
        QVERIFY(samePath("P:\\Games\\Title.nup","P:/Games/Title.nup"));
#endif
    }
    void combinedNspBuild(){
        // a build from a package that holds the game, its update and its DLC: marker version 2, the update's version, the DLC list
        QTemporaryDir tmp;const auto build=tmp.filePath("Builds/Kart 1G1U1D");const auto source=tmp.filePath("Kart [v262144] (1G+1U+1D).nsp");
        for(const auto& path:{source,build+"/Kart 1G1U1D"+executableSuffix,build+"/Kart 1G1U1D"+moduleSuffix})writeFile(path,"PFS0fixture");
        writeFile(build+"/Kart 1G1U1D_data/romfs.nsp","SWRNSP2\n"+QDir::toNativeSeparators(source).toUtf8()+"\nkeys K:/prod.keys\nromfs 800 4000\nbase 0 0 0 3 0 titlekek_02:00\n");
        Game game;QString error;QVERIFY2(gameFromExecutable(build+"/Kart 1G1U1D"+executableSuffix,&game,&error),qPrintable(error));
        QVERIFY(samePath(game.source,source));QVERIFY(game.hasBuild());QCOMPARE(game.format(),QString("DIRECT NSP"));
        QCOMPARE(game.version(),QString{});
        QByteArray nacp(0x4000,'\0');nacp.replace(0x3060,5,"1.2.3");writeFile(build+"/Kart 1G1U1D_data/control.nacp",nacp);
        QCOMPARE(game.version(),QString("1.2.3"));
        QVERIFY(BuildManager::reportsPackageDlc("DLC: this package also holds add-on content 0100152000023001; install it with --install-dlc (the launcher does)"));
        QVERIFY(!BuildManager::reportsPackageDlc("DLC: installed 0100152000023001 v65536"));
        // the first build from the package lists it; a rebuild does not put back what the user removed, and nothing is listed twice
        QCOMPARE(outputNameFor(QString::fromUtf8("Pok\xC3\xA9mon Legends - Arceus (1G+1U)")),QString("Pokemon Legends - Arceus 1G1U"));
        QCOMPARE(outputNameFor(QString::fromUtf8("\xE3\x82\xBC\xE3\x83\xAB\xE3\x83\x80")),QString("Game"));
        Game fresh=gameFromSource(source);QVERIFY(adoptPackageDlc(fresh,{}));QCOMPARE(fresh.dlc.size(),1);QVERIFY(samePath(fresh.dlc[0],source));
        QVERIFY(!adoptPackageDlc(fresh,tmp.filePath("other.nsp")));QCOMPARE(fresh.dlc.size(),1);
        fresh.dlc.clear();QVERIFY(!adoptPackageDlc(fresh,QDir::toNativeSeparators(source)));QVERIFY(fresh.dlc.isEmpty());
    }
    void nspSourceClassification(){
        QTemporaryDir tmp;const QString path=tmp.filePath("Game.nsp");
        {QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("PFS0");file.write(QByteArray(16,'\0'));}
        QString error;QVERIFY2(validSource(path,&error),qPrintable(error));
        const auto game=gameFromSource(path);QCOMPARE(game.format(),QString("DIRECT NSP"));QVERIFY(inPlacePackage(path));
        LibraryFilter filter;filter.source=1;QVERIFY(filter.matches(game));
    }
    void toolEnvironmentCarriesKeys(){
        Toolchain tools;tools.keys="D:/Keys/prod.keys";
        QCOMPARE(tools.environment().value("SWR_PROD_KEYS"),tools.keys);
    }
};
int main(int argc,char** argv) {
    // copied into fixture tool directories to exercise executable probes without another Python installation
    if(argc>1&&QByteArray(argv[1])=="-c")return 0;
    // ...and the engine's --sync-dlc (dlcInstallerRunsTheEngineSync): every package but "refused" gets a manifest line
    if(argc>3&&QByteArray(argv[1])=="-u"&&QByteArray(argv[3])=="--sync-dlc"){
        QStringList packages;QString data;
        for(int i=4;i<argc;++i){if(QByteArray(argv[i])=="--datadir"){data=QString::fromLocal8Bit(argv[i+1]);break;}packages<<QString::fromLocal8Bit(argv[i]);}
        QFile manifest(QDir(data).filePath("dlc.manifest"));if(!manifest.open(QIODevice::WriteOnly|QIODevice::Truncate))return 3;
        int failed=0;
        for(const auto& package:packages){
            if(package.contains("refused")){printf("DLC error: refused.nsp: the ticket is bound to one console\n");failed=1;continue;}
            manifest.write(("0100152000023001 65536 data "+package+"\n").toUtf8());printf("DLC: installed 0100152000023001 v65536\n");
        }
        fflush(stdout);return failed;
    }
    QCoreApplication app(argc,argv);LauncherTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "tests.moc"
