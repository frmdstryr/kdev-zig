/*
 * Copyright 2017  Emma Gospodinova <emma.gospodinova@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "duchaintest.h"

#include <QtTest/QtTest>

#include <language/backgroundparser/backgroundparser.h>
#include <language/codegen/coderepresentation.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/topducontext.h>

#include <tests/testcore.h>
#include <tests/testfile.h>
#include <tests/autotestshell.h>
#include <tests/testhelpers.h>
#include <tests/testlanguagecontroller.h>

#include "types/builtintype.h"
#include "parsesession.h"
#include "declarationbuilder.h"
#include "usebuilder.h"
#include "helpers.h"

using namespace KDevelop;

QTEST_MAIN(Zig::DUChainTest)

namespace Zig {

DUChainTest::DUChainTest(QObject* parent): QObject(parent)
{
    assetsDir = QDir(QStringLiteral(DUCHAIN_ZIG_DATA_DIR));
    if (!assetsDir.cd(QStringLiteral("data"))) {
        qFatal("Failed find data directory for test files. Aborting");
    }
}


// Parse start_line,start_col,end_line,end_col into range
static RangeInRevision rangeFromString(const QString &values) {
    if (values.trimmed() == QLatin1String("invalid")) {
        return RangeInRevision::invalid();
    }
    auto parts = values.trimmed().split(QLatin1Char(','));
    Q_ASSERT(parts.size() == 4);
    CursorInRevision start(parts[0].trimmed().toInt(), parts[1].trimmed().toInt());
    CursorInRevision end(parts[2].trimmed().toInt(), parts[3].trimmed().toInt());
    RangeInRevision range(start, end);
    return range;
}

DUContextPointer moduleInternalContext(ReferencedTopDUContext topContext) {
    return DUContextPointer(topContext);

    // // If Module context is used in nodetypes.h
    // auto decls = topContext->localDeclarations();
    // Q_ASSERT(decls.size() == 1);
    // auto mod = decls.first();
    // Q_ASSERT(mod);
    // Q_ASSERT(mod->abstractType()->modifiers() & Zig::ModuleModifier);
    // Q_ASSERT(mod->internalContext());
    // return DUContextPointer(mod->internalContext());
}

ReferencedTopDUContext parseCode(const QString &code, const QString &name)
{
    using namespace Zig;
    qDebug() << "\nparse" << name << "\n";
    // TestFile f(code, QStringLiteral(".zig"), name);
    // f.parseAndWait(TopDUContext::AllDeclarationsContextsAndUses);
    // DUChainReadLocker lock;
    // auto top = f.topContext();
    // return top;
    IndexedString document(name);
    ParseSessionData *sessionData = new ParseSessionData(document, code.toUtf8(), nullptr);
    ParseSession session(ParseSessionData::Ptr(nullptr));
    session.setData(ParseSessionData::Ptr(sessionData));
    session.parse();

    qDebug() << "Building decls";
    ZigNode root = {session.ast(), 0};
    DeclarationBuilder declarationBuilder;
    declarationBuilder.setParseSession(&session);
    ReferencedTopDUContext context = declarationBuilder.build(document, &root);

    qDebug() << "Building uses";
    UseBuilder useBuilder(document);
    useBuilder.setParseSession(&session);
    useBuilder.buildUses(&root);
    return context;
}

ReferencedTopDUContext parseFile(const QString &filename)
{
    QFile f(filename);
    Q_ASSERT(f.open(QIODevice::ReadOnly));
    QString code = QString::fromUtf8(f.readAll());
    return parseCode(code, filename);
}

ReferencedTopDUContext parseStdCode(const QString &path)
{
    QString filename = QStringLiteral("%1/%2").arg(Zig::Helper::stdLibPath(nullptr), path);
    return parseFile(filename);
}


DUContext *getInternalContext(ReferencedTopDUContext topContext, QString name, bool firstChildContext=false)
{
    if (!topContext) {
        return nullptr;
    }

    if (name.isEmpty()) {
        return topContext;
    }

    if (name.contains(QLatin1Char(','))) {
        auto parts = name.split(QLatin1Char(','));
        CursorInRevision cursor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt());
        return topContext->findContextAt(cursor, true);
    }

    QList<Declaration *> declaration = topContext->findDeclarations(QualifiedIdentifier(name));

    if (declaration.size() != 1 || !declaration.first()) {
        qDebug() << "No declaration for " << name;
        return nullptr;
    }

    DUContext *internalContext = declaration.first()->internalContext();
    if (!internalContext) {
        qDebug() << "No internal context for " << name;
        return nullptr;
    }

    if (!firstChildContext) {
        return internalContext;
    }
    Q_ASSERT(internalContext->childContexts().size() == 1);
    return internalContext->childContexts().first();
}

void DUChainTest::initTestCase()
{
    AutoTestShell::init({QStringLiteral("kdevclangsupport"), QStringLiteral("kdevzigsupport")});
    TestCore::initialize(Core::NoUi);
    //TestCore* core = new TestCore();
    //core->initialize(KDevelop::Core::NoUi);

    //DUChain::self()->disablePersistentStorage();
    //KDevelop::CodeRepresentation::setDiskChangesForbidden(true);

    // This seems to be required for the waitForUpdate to work in tests... ?
    // auto* langController = new TestLanguageController(core);
    // core->setLanguageController(langController);
    // langController->backgroundParser()->setThreadCount(4);
    // langController->backgroundParser()->abortAllJobs();
    // m_langSupport = new Zig::LanguageSupport(core);
    // langController->addTestLanguage(m_langSupport, QStringList() << QStringLiteral("text/plain"));
    // const auto languages = langController->languagesForUrl(QUrl::fromLocalFile(QStringLiteral("/foo.zig")));
    // QCOMPARE(languages.size(), 1);
    // QCOMPARE(languages.first(), m_langSupport);
}

void DUChainTest::sanityCheckFn()
{
    QString code(QStringLiteral("fn main() !void {}"));
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());

    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier(QLatin1String("main")));
    QCOMPARE(decls.size(), 1);
    Declaration *funcDeclaration = decls.first();
    QVERIFY(funcDeclaration);
    QCOMPARE(funcDeclaration->identifier().toString(), QLatin1String("main"));
    QCOMPARE(funcDeclaration->abstractType()->toString(), QLatin1String("function !void ()"));
}

void DUChainTest::sanityCheckVar()
{
    QString code(QStringLiteral("const X = 1;"));
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());

    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier(QLatin1String("X")));
    QCOMPARE(decls.size(), 1);
    Declaration *varDeclaration = decls.first();
    QVERIFY(varDeclaration);
    QCOMPARE(varDeclaration->identifier().toString(), QLatin1String("X"));
    QCOMPARE(varDeclaration->abstractType()->toString(), QLatin1String("comptime_int = 1"));
}

void DUChainTest::sanityCheckStd()
{
    // return; // Disable/
    // FIXME: This only works if modules imported inside std are already parsed...
    ReferencedTopDUContext timecontext = parseStdCode(QLatin1String("time.zig"));
    ReferencedTopDUContext stdcontext = parseStdCode(QLatin1String("std.zig"));
    QString code(QStringLiteral("const std = @import(\"std\");const time = std.time; const ns_per_us = time.ns_per_us;"));
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());
    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier(QLatin1String("std")));
    QCOMPARE(decls.size(), 1);
    // QCOMPARE(decls.first()->abstractType()->toString(), "struct std"); // std
    QVERIFY(decls.first()->abstractType()->modifiers() & Zig::ModuleModifier); // std
    decls = context->findDeclarations(Identifier(QLatin1String("time")));
    QVERIFY(decls.first()->abstractType()->modifiers() & Zig::ModuleModifier); // time
    QCOMPARE(decls.size(), 1);
    decls = context->findDeclarations(Identifier(QLatin1String("ns_per_us")));
    QCOMPARE(decls.size(), 1);
    Declaration *decl = decls.first();
    QVERIFY(decl);
    QCOMPARE(decl->identifier().toString(), QLatin1String("ns_per_us"));
    QCOMPARE(decl->abstractType()->toString(), QLatin1String("comptime_int = 1000"));
}

void DUChainTest::sanityCheckTypeInfo()
{
    // TODO: Why does waitForUpdate not do anything??
    ReferencedTopDUContext builtinctx = parseStdCode(QLatin1String("builtin.zig"));
    ReferencedTopDUContext stdcontext = parseStdCode(QLatin1String("std.zig"));
    QString code(QStringLiteral("const x = u8; const T = @typeInfo(x);"));
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());
    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier(QLatin1String("T")));
    QCOMPARE(decls.size(), 1);
    QCOMPARE(decls.first()->abstractType()->toString(), QLatin1String("std.builtin.Type::Int"));
}

void DUChainTest::sanityCheckBasicImport()
{
    ReferencedTopDUContext mod_a = parseFile(assetsDir.filePath(QLatin1String("basic_import/a.zig")));
    ReferencedTopDUContext mod_b = parseFile(assetsDir.filePath(QLatin1String("basic_import/b.zig")));
    DUChainReadLocker lock;

    qDebug() << "mod_a decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_a)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    qDebug() << "mod_b decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_b)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    auto decls = mod_b->findDeclarations(Identifier(QLatin1String("a")));
    QCOMPARE(decls.size(), 1);
    QCOMPARE(decls.first()->abstractType()->toString(), assetsDir.filePath(QLatin1String("basic_import/a.zig")));
    QVERIFY(decls.first()->abstractType()->modifiers() & Zig::ModuleModifier); // std
    decls = mod_b->findDeclarations(Identifier(QLatin1String("ImportedA")));
    QCOMPARE(decls.size(), 1);
    Q_ASSERT(decls.first()->internalContext());
    // auto importedContexts = decls.first()->internalContext()->importedParentContexts();
    // QVERIFY(importedContexts.size() > 0);
    // auto importedContext = importedContexts.last().indexedContext().data();
    // QVERIFY(importedContext);
    // QVERIFY(importedContext->owner());
    // QCOMPARE(
    //     importedContext->owner()->toString(),
    //     assetsDir.filePath("basic_import/a.zig")
    // );
    //QCOMPARE(decls.first()->abstractType()->toString(), assetsDir.filePath("a.zig") + "::A");

    decls = mod_b->findDeclarations(Identifier(QLatin1String("c")));
    QCOMPARE(decls.size(), 1);
    QCOMPARE(decls.first()->abstractType()->toString(), QLatin1String("u8"));
}

void DUChainTest::sanityCheckDuplicateImport()
{
    ReferencedTopDUContext mod_a = parseFile(assetsDir.filePath(QLatin1String("duplicate_import/config_a.zig")));
    ReferencedTopDUContext mod_b = parseFile(assetsDir.filePath(QLatin1String("duplicate_import/config_b.zig")));
    ReferencedTopDUContext mod_main = parseFile(assetsDir.filePath(QLatin1String("duplicate_import/main.zig")));
    DUChainReadLocker lock;

    qDebug() << "mod a decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_a)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    qDebug() << "mod b decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_b)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    qDebug() << "main decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_main)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }


    auto config_decls_a = mod_a->findDeclarations(Identifier(QLatin1String("Config")));
    QCOMPARE(config_decls_a.size(), 1);
    QCOMPARE(config_decls_a.first()->abstractType()->toString(), QLatin1String("Config"));
    auto config_decls_b = mod_b->findDeclarations(Identifier(QLatin1String("Config")));
    QCOMPARE(config_decls_b.size(), 1);
    QCOMPARE(config_decls_b.first()->abstractType()->toString(), QLatin1String("Config"));

    auto config_a = mod_main->findDeclarations(Identifier(QLatin1String("config_a")));
    QCOMPARE(config_a.size(), 1);
    auto config_b = mod_main->findDeclarations(Identifier(QLatin1String("config_b")));
    QCOMPARE(config_b.size(), 1);

    QVERIFY(config_a.first()->abstractType() != config_b.first()->abstractType());

    auto x = mod_main->findDeclarations(Identifier(QLatin1String("x")));
    QCOMPARE(x.size(), 1);
    QCOMPARE(x.first()->abstractType()->toString(), QLatin1String("bool"));
    auto y = mod_main->findDeclarations(Identifier(QLatin1String("y")));
    QCOMPARE(y.size(), 1);
    QCOMPARE(y.first()->abstractType()->toString(), QLatin1String("i8"));


}

void DUChainTest::sanityCheckThisImport()
{
    ReferencedTopDUContext mod_a = parseFile(assetsDir.filePath(QLatin1String("this_import/A.zig")));
    ReferencedTopDUContext mod_main = parseFile(assetsDir.filePath(QLatin1String("this_import/main.zig")));
    DUChainReadLocker lock;

    qDebug() << "mod A decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_a)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    qDebug() << "main decls are:";
    for (const KDevelop::Declaration *decl : moduleInternalContext(mod_main)->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    auto A = mod_a->findDeclarations(Identifier(QLatin1String("A")));
    QCOMPARE(A.size(), 1);
    QCOMPARE(A.first()->abstractType()->toString(), assetsDir.filePath(QLatin1String("this_import/A.zig")));
    auto x = mod_main->findDeclarations(Identifier(QLatin1String("a")));
    QCOMPARE(x.size(), 1);
    QCOMPARE(x.first()->abstractType()->toString(), assetsDir.filePath(QLatin1String("this_import/A.zig")));
}


void DUChainTest::cleanupTestCase()
{
    TestCore::shutdown();
}

void DUChainTest::testVarBindings()
{
    QFETCH(QString, code);
    QFETCH(QString, bindings);
    QFETCH(QString, contextName);
    qDebug() << "Code:" << code;
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());

    DUChainReadLocker lock;
    DUContext *internalContext = getInternalContext(context, contextName);
    QVERIFY(internalContext);

    qDebug() << "Decls are:";
    for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    for (const QString &binding : bindings.split(QLatin1Char(','))) {
        qDebug() << "  checking for" << binding.trimmed();
        QCOMPARE(internalContext->findLocalDeclarations(Identifier(binding.trimmed())).size(),  1);
    }
}

void DUChainTest::testVarBindings_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("bindings");
    QTest::addColumn<QString>("contextName");

    QTest::newRow("simple var") << "var x = 1;" << "x" << "";
    QTest::newRow("simple const") << "const y = 2;" << "y" << "";
    QTest::newRow("simple var typed") << "var y: u8 = 2;" << "y" << "";
    QTest::newRow("multiple vars") << "var x = 1;\nvar y = 2;" << "x, y"  << "";
    QTest::newRow("fn and var") << "var x = 1;\npub fn foo() void {}" << "x, foo" << "" ;
    QTest::newRow("fn var") << "pub fn main() void {\n  var y: u8 = 2;\n _ = y;\n}" << "y" << "1,0";
    QTest::newRow("struct decl") << "const Foo = struct {};" << "Foo" << "";
    QTest::newRow("fn multiple vars") << "pub fn main() void {\n var y: u8 = 2;\n var x = y; _ = x;\n}" << "y, x" << "1,0";
    QTest::newRow("struct fn") << "const Foo = struct { pub fn bar() void {}};" << "bar" << "Foo";
    QTest::newRow("struct var") << "const Foo = struct { var x = 1; };" << "x" << "Foo";
    QTest::newRow("struct vars") << "const Foo = struct { var x = 1; const y: u8 = 0;};" << "x, y" << "Foo";
    QTest::newRow("struct field") << "const Foo = struct { a: u8, };" << "a" << "Foo";
    QTest::newRow("test decl") << "test \"foo\" {  }" << "test foo"  << "";
    QTest::newRow("if capture") << "test {var opt: ?u8 = null;\n if (opt) |x| {\n_ = x;\n} }" << "x" << "1,13";
    QTest::newRow("catch capture") << "test {\nsomething() catch |err| {_ = err;}\n}" << "err" << "1,23";
    QTest::newRow("catch capture expr") << "test {\nconst n = something() catch |err| 0; \n}" << "err" << "1,23";
    // Interal context ?
    // QTest::newRow("fn var in if") << "main" << "pub fn main() void { if (true) { var i: u8 = 0;} }" << QStringList { "i" };

}

void DUChainTest::testVarUsage()
{
    QFETCH(QString, code);
    QFETCH(QString, container);
    QFETCH(QString, uses);
    qDebug() << "Code:" << code;
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());

    DUChainReadLocker lock;
    uint32_t i = 0;
    for (const QString &param : uses.split(QLatin1Char(';'))) {
        QStringList parts = param.split(QLatin1Char('/'));
        QString use = parts[0].trimmed();
        RangeInRevision expectedRange = rangeFromString(parts[1].trimmed());
        qDebug() << "Checking for use of" << use;
        DUContext *internalContext = getInternalContext(context, container);
        QVERIFY(internalContext);
        //qDebug() << "Local decls at:" << cursor;
        qDebug() << "Locals are:";
        for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
            qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
        }

        auto decls = internalContext->findDeclarations(Identifier(use));
        qDebug() << "Decls for ident" << use;
        for (const KDevelop::Declaration *decl : decls) {
            qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
        }
        QVERIFY(decls.size() >  0);
        if (expectedRange.isValid()) {
            QCOMPARE(decls.first()->uses().size(),  1);
            QVector<RangeInRevision> useRanges = decls.first()->uses().values().at(0);
            QCOMPARE(useRanges.size(),  1);
            QCOMPARE(useRanges.at(0), expectedRange);
        } else {
            QCOMPARE(decls.first()->uses().size(),  0);
        }

        i += 1;
    }
}

void DUChainTest::testVarUsage_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("uses");
    QTest::addColumn<QString>("container");


    QTest::newRow("struct init") << "const Foo = struct{};\ntest {\n var x = Foo{};\n}" << "Foo/2,9,2,12" << "3,0";
    QTest::newRow("var") << "test{\n const b=1;\n const a = b;\n}" << "b/2,11,2,12" << "3,0";
    QTest::newRow("var type") << "const A = struct {};\nvar a: A = undefined;\n" << "A/1,7,1,8" << "2,0";

    QTest::newRow("var out of order") << "const a = b;\n const b=1;\n" << "b/0,10,0,11" << "2,0";
    QTest::newRow("fn call") << "pub fn main() void {}\ntest {\n main();\n}" << "main/2,1,2,5" << "3,0";
    QTest::newRow("fn arg") << "pub fn main(a: u8) u8 {\nconst b=1; return a;\n}" << "a/1,18,1,19" << "2,0";
    QTest::newRow("field access") << "const A = struct{f: u8=0};\ntest{\n var a = A{};\n var b = a.f;\n}" << "a/3,9,3,10" << "4,0";
    QTest::newRow("struct field") << "const A = struct{f: u8=0};\ntest{\n var a = A{};\n var b = a.f;\n}" << "f/3,11,3,12" << "A";
    QTest::newRow("struct fn") << "const A = struct{f: u8=0, pub fn foo() void {}};\ntest{\n var a = A{};\n var b = a.foo();\n}" << "foo/3,11,3,14" << "A";
    QTest::newRow("struct field type") << "const A = struct{b: Bar};\nconst Bar = struct{v: u8};\n" << "Bar/0,20,0,23" << "";

    QTest::newRow("array access") << "var a = [2]u8;\nconst b = a[1];\n" << "a/1,10,1,11" << "2,0";
    QTest::newRow("array index") << "const i = 0;\nvar a = [2]u8;\nconst b = a[i];\n" << "i/2,12,2,13" << "3,0";
    // QTest::newRow("enum use") << "const Status = enum {Ok, Error};\nvar status: Status = .Ok;\n" << "Ok/2,22,2,24" << "Status";

    // Make sure out of order does not work in fn's or tests
    QTest::newRow("fn var order") << "pub fn main() void {\nconst a = b; const b=1; \n}" << "a/invalid" << "2,0";
    QTest::newRow("test var order") << "test {\nconst a = b; const b=1; \n}" << "a/invalid" << "2,0";


        // QTest::newRow("builtin call") << CursorInRevision(-1, -1)
    //     << "const a = @min(1, 2);\n" << QStringList { "@min/2,10,2,14" };

}



void DUChainTest::testVarType()
{
    QFETCH(QString, code);
    QFETCH(QString, var);
    QFETCH(QString, type);
    QFETCH(QString, container);

    qDebug() << "Code:" << code;

    if (code.contains(QStringLiteral("cInclude"))) {
        DUChain::self()->updateContextForUrl(IndexedString("/usr/include/locale.h"), KDevelop::TopDUContext::ForceUpdate);
        ICore::self()->languageController()->backgroundParser()->parseDocuments();
        DUChain::self()->waitForUpdate(IndexedString("/usr/include/locale.h"), KDevelop::TopDUContext::ForceUpdate);
    }

    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));
    QVERIFY(context.data());

    DUChainReadLocker lock;
    DUContext *internalContext = getInternalContext(context, container);
    QVERIFY(internalContext);

    qDebug() << "Locals are:";
    for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
        qDebug() << "  name" << decl->identifier();
        if (decl->abstractType()) {
            qDebug() << " type" << decl->abstractType()->toString();
        }
    }
    auto decls = internalContext->findDeclarations(Identifier(var));
    QCOMPARE(decls.size(),  1);
    const KDevelop::Declaration *decl = decls.first();
    qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    QCOMPARE(decl->abstractType()->toString(), type);
}

void DUChainTest::testVarType_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("var");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("container");

    QTest::newRow("var u8") << "var x: u8 = 0;" << "x" << "u8" << "";
    QTest::newRow("const u8") << "const x: u8 = 0;" << "x" << "u8 = 0" << "";
    QTest::newRow("const bool") << "const x = true;" << "x" << "bool = true" << "";
    QTest::newRow("var bool") << "var x: bool = undefined;" << "x" << "bool" << "";
    QTest::newRow("comp int") << "const x = 1;" << "x" << "comptime_int = 1" << "";
    QTest::newRow("comp int hex") << "const x = 0xff;" << "x" << "comptime_int = 0xff" << "";
    QTest::newRow("comp int bin") << "const x = 0b11;" << "x" << "comptime_int = 0b11" << "";
    QTest::newRow("comp float") << "const x = 1.1;" << "x" << "comptime_float = 1.1" << "";
    QTest::newRow("const str") << "const x = \"abc\";" << "x" << "*const [3:0]u8 = \"abc\"" << "";
    QTest::newRow("const str index") << "const y = \"abc\"; const x = y[0];" << "x" << "const u8 = a" << "";
    QTest::newRow("volatile ptr") << "var io: *volatile u32 = @ptrFromInt(0x400000);" << "io" << "*volatile u32" << "";
    QTest::newRow("char lit") << "const x = 'a';" << "x" << "u8 = a" << "";
    QTest::newRow("multiline str") << "const x = \n\\\\abc\n;" << "x" << "*const [4:0]u8 = \"abc\n\"" << "";

    // QTest::newRow("char lit esc") << "const x = '\n';" << "x" << "u8 = \n" << "";
    QTest::newRow("var *u8") << "var x: *u8 = 0;" << "x" << "*u8" << "";
    QTest::newRow("var ?u8") << "var x: ?u8 = 0;" << "x" << "?u8" << "";
    QTest::newRow("var ?*u8") << "var x: ?*u8 = 0;" << "x" << "?*u8" << "";
    QTest::newRow("void block") << "const x = {};" << "x" << "void" << "";
    QTest::newRow("enum") << "const Day = enum{Mon, Tue};" << "Day" << "Day" << "";
    QTest::newRow("enum value") << "const Day = enum{Mon, Tue}; const mon = Day.Mon;" << "mon" << "Day.Mon" << "";
    QTest::newRow("inferred enum") << "const Day = enum{Mon, Tue}; const x: Day = .Mon;" << "x" << "Day.Mon" << "";
    QTest::newRow("switch inferred value") << "const Day = enum{Mon, Tue}; const x: Day = switch (1) {0 => .Mon, 1=> .Tue};" << "x" << "Day.Tue" << "";
    QTest::newRow("error set") << "const E = error{A, B};" << "E" << "E" << "";
    QTest::newRow("error set value") << "const E = error{A, B}; const x = E.A;" << "x" << "E.A" << "";
    QTest::newRow("union") << "const Payload = union {int: u32, float: f32};" << "Payload" << "Payload" << "";
    QTest::newRow("union field") << "const Payload = union {int: u32, float: f32}; const x = Payload{.int=1};" << "x" << "Payload.int" << "";
    QTest::newRow("union enum") << "const Payload = union(enum) {int: u32, float: f32};" << "Payload" << "Payload" << "";
    QTest::newRow("enum arg") << "const Day = enum(u8){Mon = 0, Tue}; const mon = Day.Mon;" << "mon" << "Day.Mon = 0" << "";
    // FIXME: QTest::newRow("enum int") << "const Status = enum{Ok = 1, Error = 0};" << "Ok" << "Status" << "";
    QTest::newRow("struct") << "const Foo = struct {a: u8};" << "Foo" << "Foo" << "";
    QTest::newRow("struct field") << "const Foo = struct {\n a: u8\n};" << "a" << "u8" << "Foo";
    QTest::newRow("struct field enum") << "const S = enum {Ok, Err}; const A = struct {\n s: S\n};" << "s" << "S" << "A";
    QTest::newRow("struct field shadowed") << "const a = struct{ b: bool}; const Foo = struct {\n a: ?a.b\n};" << "a" << "?bool" << "Foo";
    QTest::newRow("fn void") << "pub fn main() void {}" << "main" << "function void ()"<< "";
    QTest::newRow("fn !void") << "pub fn main() !void {}" << "main" << "function !void ()"<< "";
    QTest::newRow("fn u8") << "pub fn main() u8 {}" << "main" << "function u8 ()"<< "";
    QTest::newRow("fn arg") << "pub fn main(a: bool) void {}" << "main" << "function void (bool)"<< "";
    QTest::newRow("fn anytype") << "pub fn main(a: anytype) void {}" << "main" << "function void (anytype)"<< "";
    QTest::newRow("extern fn") << "extern fn add(a: u8, b: u8) u8;" << "add" << "function u8 (u8, u8)"<< "";
    // TOOD: Why is it not *const ??
    QTest::newRow("extern fn ptr") << "extern fn foo_add(a: u8, b: u8) u8; const add = foo_add;" << "add" << "function u8 (u8, u8)"<< "";
    //QTest::newRow("struct field fn ptr") << "const Math = struct { add: fn(a: u8, b: u8) u8}; test { const m = Math{}; const y = m.add(1, 2);\n" << "y" << "u8"<< "1, 0";
    QTest::newRow("fn err!void") << "const WriteError = error{EndOfStream};\npub fn main() WriteError!void {}" << "main" << "function WriteError!void ()"<< "";
    QTest::newRow("var struct") << "const Foo = struct {a: u8};\ntest {\nvar f = Foo{};}" << "f" << "Foo" << "2,0";
    QTest::newRow("field access") << "const Foo = struct {a: u8=0};\ntest {\nvar f = Foo{}; var b = f.a;\n}" << "b" << "u8" << "3,0";
    QTest::newRow("field ptr") << "const Foo = struct {a: u8=0};\ntest {\nvar f = &Foo{}; var b = f.a;\n}" << "b" << "u8" << "3,0";
    QTest::newRow("fn call") << "pub fn foo() f32 { return 0.0; }\ntest {\nvar f = foo();\n}" << "f" << "f32" << "3,0";
    QTest::newRow("var addr of") << "const Foo = struct {a: u8};\ntest {\nvar f = Foo{}; var x = &f;\n}" << "x" << "*Foo" << "3,0";
    QTest::newRow("ptr deref") << "test {\nvar buf: *u8 = undefined; var x = buf.*;\n}" << "x" << "u8" << "2,0";
    QTest::newRow("unwrap optional") << "var x: ?u8 = 0; const y = x.?;" << "y" << "u8" << "";
    QTest::newRow("unwrap optional ptr") << "var x: *?u8 = undefined; const y = x.?;" << "y" << "u8" << "";
    QTest::newRow("unwrap optional ptr field")
        << "const A = struct {value: u8}; const B = struct {a: ?*A = null};"
           "var b = B{}; var ptr = &b; const y = ptr.a.?;" << "y" << "*A" << "";
    QTest::newRow("bool >") << "var x = 1 > 2;" << "x" << "bool" << "";
    QTest::newRow("bool >=") << "var x = 1 >= 2;" << "x" << "bool" << "";
    QTest::newRow("bool <") << "var x = 1 < 2;" << "x" << "bool" << "";
    QTest::newRow("bool <=") << "var x = 1 <= 2;" << "x" << "bool" << "";
    QTest::newRow("bool ==") << "var x = 1 == 2;" << "x" << "bool" << "";
    QTest::newRow("bool !=") << "var x = 1 != 2;" << "x" << "bool" << "";
    QTest::newRow("add") << "var x: u8 = 1; var y: u8 = 2; var z = x + y;" << "z" << "u8" << "";
    QTest::newRow("add invalid types") << "var x: u8 = 1; var y: i8 = 2; var z = x + y;" << "z" << "mixed" << "";
    QTest::newRow("add comptime rhs") << "var x: u8 = 1; var y = 2 + x;" << "y" << "u8" << "";
    QTest::newRow("add comptime lhs") << "var x: u8 = 1; var y = x + 2;" << "y" << "u8" << "";
    QTest::newRow("negate signed") << "var x: i8 = 1; var y = -x;" << "y" << "i8" << "";
    QTest::newRow("negate comptime") << "var x = 1; var y = -x;" << "y" << "comptime_int = -1" << "";
    QTest::newRow("negate unsigned") << "var x: u8 = 1; var y = -x;" << "y" << "mixed" << "";
    QTest::newRow("negate float") << "var x: f32 = 1; var y = -x;" << "y" << "f32" << "";
    QTest::newRow("math expr") << "var x: f32 = 1; var y = (1 + -x);" << "y" << "f32" << "";
    QTest::newRow("math expr 2") << "var x: f32 = 1; var y = (1/x + -2*x);" << "y" << "f32" << "";
    QTest::newRow("math expr infer lhs") << "var x: bool = undefined; const v = @as(u8, @intFromBool(x)) | @intFromBool(1);" << "v" << "u8" << "";
    //QTest::newRow("math expr infer rhs") << "var x: bool = undefined; const v = @intFromBool(1) | @as(u8, @intFromBool(x));" << "v" << "u8" << "";
    QTest::newRow("try") << "pub fn main() !f32 {return 1;}\ntest{\nvar x = try main(); \n}" << "x" << "f32"<< "3,0";
    QTest::newRow("orelse") << "var x: ?i8 = null; var y = x orelse 1;" << "y" << "i8" << "";
    QTest::newRow("orelse bad") << "var x: i8 = 0; var y = x orelse 1;" << "y" << "mixed" << "";
    // Note: Using an undefined var in the if expr forces merging
    QTest::newRow("if expr 1") << "var x = if (true) 1 else 2;" << "x" << "comptime_int = 1" << "";
    QTest::newRow("if expr 2") << "var x = if (false) 1 else 2;" << "x" << "comptime_int = 2" << "";
    QTest::newRow("if expr 3") << "var x: ?u8 = 0; var y = if (x) |z| z else 2;" << "y" << "u8" << "";
    QTest::newRow("if expr 4") << "const T = if (1 > 2) u8 else u32;" << "T" << "u32" << "";
    QTest::newRow("if expr str") << "const s = if (x) \"true\" else \"false\";" << "s" << "*const [:0]u8" << "";
    //QTest::newRow("if expr 4") << "var y = if (null) |z| z else 2;" << "y" << "comptime_int = 2" << "";
    QTest::newRow("if expr merge bool") << "var y = if (x > 2) true else !true;" << "y" << "bool" << "";
    QTest::newRow("if expr merge 1") << "var y = if (x > 2) null else false;" << "y" << "?bool" << "";
    QTest::newRow("if expr merge 2") << "var a: ?u8 = 0; var y = if (a) |b| b else 0;" << "y" << "u8" << "";
    QTest::newRow("if expr merge 3") << "var a: u8 = 0; var y = if (x > 2) a else null;" << "y" << "?u8" << "";
    QTest::newRow("if expr merge 4") << "var a: u8 = 0; var y = if (x > 2) a else 0;" << "y" << "u8" << "";
    QTest::newRow("if expr merge 5") << "var a: ?u8 = 0; var b: u8 = 0; var y = if (x > 2) a else b;" << "y" << "?u8" << "";
    QTest::newRow("if expr merge 6") << "var a: ?u8 = 0; var b: u8 = 0; var y = if (x > 2) b else a;" << "y" << "?u8" << "";

    QTest::newRow("bool not 1") << "const y = !true;" << "y" << "bool = false" << "";
    QTest::newRow("bool not 2") << "const y = !false;" << "y" << "bool = true" << "";
    QTest::newRow("bool not 3") << "var x: bool = undefined; const y = !x;" << "y" << "bool" << "";
    QTest::newRow("bool expr 1") << "const y = if (true and false) 1 else 2;" << "y" << "comptime_int = 2" << "";
    QTest::newRow("bool expr 2") << "const y = if (true or false) 1 else 2;" << "y" << "comptime_int = 1" << "";
    QTest::newRow("bool expr 3") << "const y = (true and !false);" << "y" << "bool = true" << "";
    QTest::newRow("bool expr 4") << "var x: bool = undefined; const y = if (x and false) 1 else 2;" << "y" << "comptime_int = 2" << "";
    // Result depends on x
    QTest::newRow("bool expr 5") << "var x: bool = undefined; const y = if (x or false) 1 else 2;" << "y" << "mixed" << "";
    QTest::newRow("fn ptr") << "const x = fn() bool;" << "x" << "*const function bool ()" << "";

    QTest::newRow("var fixed array") << "var x: [2]u8 = undefined;" << "x" << "[2]u8" << "";
    QTest::newRow("var fixed array type") << "const A = struct {}; var x: [2]A = undefined;" << "x" << "[2]A" << "";
    QTest::newRow("array access") << "var x: [2]u8 = undefined; var y = x[0];" << "y" << "u8" << "";
    QTest::newRow("array len") << "var x: [2]u8 = undefined; var y = x.len;" << "y" << "usize" << "";
    QTest::newRow("array slice") << "var x: [2]u8 = undefined; var y = x[0..];" << "y" << "[]u8" << "";
    QTest::newRow("array init") << "var x = [_]u8{1, 2};" << "x" << "[2]u8" << "";
    QTest::newRow("array init comma") << "var x = [_]u8{1, 2, 3,};" << "x" << "[3]u8" << "";
    QTest::newRow("array init one") << "var x = [_]u8{1};" << "x" << "[1]u8" << "";
    QTest::newRow("array slice open") << "const a = [2]f32{1.0, 2.0}; var ptr = &a; const x = ptr[0..];" << "x" << "[]f32" << "";
    QTest::newRow("array init one comma") << "var x = [_]u8{1,};" << "x" << "[1]u8" << "";
    QTest::newRow("array cat 1") << "const x = \"abc\" ++ \"1234\";" << "x" << "*const [7:0]u8 = \"abc1234\"" << "";
    QTest::newRow("array cat 2") << "const a = [2]u32{1,2}; const b = [3]u32{3, 4, 5}; const x = a ++ b;" << "x" << "[5]u32" << "";
    QTest::newRow("array cat 3") << "const x = \"a\" ++ \n\\\\bc\n; " << "x" << "*const [4:0]u8 = \"abc\n\"" << "";

    QTest::newRow("c ptr array access") << "const ptr: [*]const f32 = undefined; const x = ptr[0];" << "x" << "const f32" << "";

    QTest::newRow("array ptr struct fn") << "const A = struct { pub fn foo() bool{} }; var x: [2]*A = undefined; var y = x[0].foo();" << "y" << "bool" << "";
    QTest::newRow("sentinel array") << "var x: [100:0]u8 = undefined;" << "x" << "[100:0]u8" << "";
    QTest::newRow("ptr type aligned") << "var x: []u8 = undefined;" << "x" << "[]u8" << "";
    QTest::newRow("ptr type aligned ptr") << "var x: *align(4)u8 = undefined;" << "x" << "*u8" << "";
    QTest::newRow("ptr type aligned array ptr") << "var x: [*]u8 = undefined;" << "x" << "[*]u8" << "";
    QTest::newRow("ptr extern opaque") << "extern fn currentContext() ?Context; const Context = *opaque {};" << "currentContext" << "function ?*anon opaque 8 ()" << "";

    QTest::newRow("for range") << "test{for (0..10) |y| {\n}}" << "y" << "usize" << "1,0";
    QTest::newRow("for slice") << "test{var x: [2]i8 = undefined; for (x) |y| {\n}}" << "y" << "i8" << "1,0";
    QTest::newRow("for slice ptr") << "test{var x: [2]i8 = undefined; for (x[0..]) |*y| {\n}}" << "y" << "*i8" << "1,0";
    QTest::newRow("for ptr") << "test{var x: [2]i8 = undefined; for (&x) |*y| {\n}}" << "y" << "*i8" << "1,0";
    QTest::newRow("for ptr const") << "test{var x: [2]i8 = undefined; for (&x) |y| {\n}}" << "y" << "i8" << "1,0";
    QTest::newRow("for multiple") << "test{var x: [2]i8 = undefined; for (&x, 0..) |y, i| {\n}}" << "i" << "usize" << "1,0";
    QTest::newRow("for ptr multiple") << "test{var a = [2]i8{1,2}; var b = [2]u8{1,2}; for (&a, &b) |x, *y| {\n}}" << "y" << "*u8" << "1,0";
    QTest::newRow("catch")
        << "pub fn write() !u8 {return 1;}\n"
           "test{const x = write() catch 0;\n}" << "x" << "u8" << "2,0";
    QTest::newRow("catch capture")
        << "const WriteError = error{EndOfStream};\n"
           "pub fn write() WriteError!void {}\n"
           "test{write() catch |err| {\n}}" << "err" << "WriteError" << "3,0";

    // Builtin calls
    QTest::newRow("@This()") << "const Foo = struct { const Self = @This();\n};" << "Self" << "Foo" << "1,0";
    QTest::newRow("@sizeOf()") << "const Foo = struct { a: u8, }; test {var x = @sizeOf(Foo);\n}" << "x" << "comptime_int" << "1,0";
    QTest::newRow("@as()") << "test{var x = @as(u8, 1);\n}" << "x" << "u8 = 1" << "1,0";
    QTest::newRow("@as(u32) << 2") << "test{var x = @as(u32, 0xFF) << 8;\n}" << "x" << "u32 = 0xff00" << "1,0";
    QTest::newRow("@min()") << "test{var x: u32 = 1; var y = @min(x, 1);\n}" << "x" << "u32" << "1,0";
    QTest::newRow("@hasField()") << "const Foo = struct {a: u8}; test{var x = @hasField(Foo, \"a\");\n}" << "x" << "bool" << "1,0";
    QTest::newRow("@field()") << "const Foo = struct {a: u8}; test{var x = Foo{}; var y = @field(x, \"a\");\n}" << "y" << "u8" << "1,0";
    QTest::newRow("@field() expr") << "const Foo = struct {a: u8}; test{const f = \"a\"; var x = Foo{}; var y = @field(x, f);\n}" << "y" << "u8" << "1,0";
    QTest::newRow("@tagName()") << "const Foo = enum{A, B}; test{var x = @tagName(Foo.A);\n}" << "x" << "[:0]const u8" << "1,0";
    QTest::newRow("vector") << "const x = @Vector(4, f32);" << "x" << "@Vector(4, f32)" << "";
    QTest::newRow("vector access") << "const Vec = @Vector(4, f32); const v: Vec = undefined; const x = v[0];" << "x" << "f32" << "";
    QTest::newRow("vector reduce") << "const v: @Vector(4, f32) = undefined; const x = @reduce(.Max, v);" << "x" << "f32" << "";
    QTest::newRow("@cImport") << "const c = @cImport({@cInclude(\"/usr/include/locale.h\")}); const f = c.setlocale;" << "f" << "function char* (int, const char*)" << "";



    QTest::newRow("cast @boolFromInt()") << "const y: u8 = 7; const x = @boolFromInt(y);" << "x" << "bool = true" << "";
    QTest::newRow("cast @boolFromInt() 2") << "const y: i8 = 1; const x = @boolFromInt(-y);" << "x" << "bool = true" << "";
    QTest::newRow("cast @intFromBool()") << "const x: u8 = @intFromBool(true);" << "x" << "u8 = 1" << "";
    QTest::newRow("cast @intFromBool() 2") << "const x: i8 = @intFromBool(false);" << "x" << "i8 = 0" << "";
    QTest::newRow("@fieldParentPtr()") <<
        "const Point = struct {x: i32=0, y: i32=0};\n"
        "test{var point = Point{}; const p: *Point = @fieldParentPtr(\"x\", &point.x);\n}"
        << "p" << "*Point" << "2,0";

    // TODO: Order of decls matters but should not...
    QTest::newRow("nested struct") << "const Foo = struct {bar: Bar = .{}, const Bar = struct {a: u8};}; test{\nvar f = Foo.Bar{};\n}" << "f" << "Foo::Bar" << "2,0";
    QTest::newRow("nested struct field") << "const Foo = struct {const Bar = struct {a: u8}; bar: Bar = .{}, }; test{\nvar f = Foo{}; var y = f.bar.a;\n}" << "y" << "u8" << "2,0";
    QTest::newRow("struct fn arg") <<
        "const Bar = struct {\n"
        "  a: u8, \n"
        "  pub fn copy(other: Bar) Bar {\n"
        "      const b = other.a;\n"
        "      return Bar{.a=b};\n"
        "  }"
        "};" << "b" << "u8" << "5,0";
    QTest::newRow("nested struct fn arg") <<
        "const Foo = struct {\n"
        "  const Bar = struct {\n"
        "    a: u8, \n"
        "    pub fn nestedCopy(other: Bar) Bar {\n"
        "        const b = other.a;\n"
        "        return Bar{.a=b};\n"
        "    }"
        "  }; "
        "  bar: Bar = .{}, "
        "};" << "b" << "u8" << "5,0";
    QTest::newRow("nested struct field after") << "const Foo = struct {bar: Bar = .{}, const Bar = struct {a: u8}; }; test{\nvar f = Foo{}; var y = f.bar.a;\n}" << "y" << "u8" << "2,0";

    QTest::newRow("if capture") << "test { var x: ?u8 = 0; if (x) |y| {\n} }" << "y" << "u8" << "1,0";
    QTest::newRow("while capture") << "test { var x: ?u8 = 0; while (x) |y| {\n} }" << "y" << "u8" << "1,0";

    // Imported namespaces
    QTest::newRow("no usingnamespace") << "const A = struct {const a = 1;}; const B = struct { var x = a;\n};" << "x" << "mixed" << "B";
    QTest::newRow("usingnamespace") << "const A = struct {const a = 1;}; const B = struct { usingnamespace A; var x = a;\n};" << "x" << "comptime_int" << "B";

    QTest::newRow("switch") << "var a: u1 = 1; const x = switch (a) {0=> \"zero\", 1=> \"one\"};" << "x" << "*const [:0]u8" << "x";

    QTest::newRow("var order") << "const A = 1 << B; const B = @as(u32, 1);\n" << "A" << "u32" << "";
    QTest::newRow("aliased struct") <<
       "const geom = struct { const Point = struct { x: i8, y: i8}; };\n"
       "const Point = geom.Point;\n"
       "var p = Point{}; const value = p.x; " << "value" << "i8" << "";
    QTest::newRow("aliased field init") <<
       "const geom = struct { const Point = struct { x: i8, y: i8}; };\n"
       "var p = geom.Point{}; " << "p" << "geom::Point" << "";
    QTest::newRow("aliased struct out of order") <<
       "const Point = geom.Point;\n"
       "var p = Point{};\n"
       "const geom = struct { const Point = struct { x: i8, y: i8}; };" << "p" << "geom::Point" << "";
    QTest::newRow("import struct missing") <<
       "const geom = @import(\"geom\");\n"
       "const Point = geom.Point;\n"
       "var p = Point{};\n" << "p" << "mixed" << "";

    QTest::newRow("field in fn") <<
       "const geom = struct { const Point = struct {x: i8, y: i8}; };\n"
       "pub fn add() void {\n"
       "  var a = geom.Point{};\n"
       "  const b = a.x + a.y;\n"
       "}"<< "b" << "i8" << "3,0";

    QTest::newRow("comptime type simple") << "pub fn Foo() type { return u8; } test{\nconst x = Foo();\n}" << "x" << "u8" << "2,0";
    QTest::newRow("comptime fn arg return") << "pub fn add(comptime T: type, a: T, b: T) T { return a + b; } test{\nconst x = add(u32, 1, 2);\n}" << "x" << "u32" << "2,0";
    QTest::newRow("comptime fn arg return error") << "pub fn parseInt(comptime T: type, a: []const u8) !T { return 1; } test{\nconst x = try parseInt(u32, 1);\n}" << "x" << "u32" << "2,0";
    QTest::newRow("comptime block return") << "pub fn Foo() type { comptime {return u8;} } test{\nconst x = Foo();\n}" << "x" << "u8" << "2,0";
    QTest::newRow("comptime block struct") << "pub fn Foo() type { comptime { const T = u32; return struct {a: T};} } test{\nconst x = Foo();\n}" << "x" << "Foo::anon struct 8" << "2,0";
    QTest::newRow("comptime struct") <<
        "pub fn Foo(comptime T: type) type { return struct {a: T}; }\n"
        "test{const x = Foo(u8);\n}" << "x" << "Foo::anon struct 7" << "2,0";
    QTest::newRow("comptime struct fn") <<
        "pub fn Bar() type { return Foo(u8); } \n"
        "pub fn Foo(comptime T: type) type { return struct {a: T}; } \n"
        "test{\nconst B = Bar(u8);\n}" << "B" << "Foo::anon struct 15" << "3,0";
    QTest::newRow("recursive comptime struct fn") <<
        "pub fn Foo(comptime T: ?type) type { if (T == null) {return Foo(u8);} return struct {a: T}; } \n"
        "test{\nconst F = Foo(null);\n}" << "F" << "Foo::anon struct 17" << "3,0";
    QTest::newRow("comptime switch case")
        << "const Kind = enum {Char, Int}; test{\n"
           "const x = Kind.Int; const y = switch (x) {.Char=>u8,.Int=>i32};\n}"
           << "y" << "i32" << "2,0";
    QTest::newRow("comptime typed field")
        << "const Kind = enum {Char, Int}; test{\n"
           "const y: Kind = @field(Kind, \"Int\");\n}"
           << "y" << "Kind.Int" << "2,0";

    // TODO: Need to be able to specialize the "type"
    QTest::newRow("comptime type arg") << "pub fn Foo(comptime T: type) type { return T; } test{\nconst x = Foo(u32);\n}" << "x" << "u32" << "2,0";
}


void DUChainTest::testProblems()
{
    QFETCH(QString, code);
    QFETCH(QStringList, problems);
    QFETCH(QString, contextName);

    qDebug() << "Code:" << code;
    ReferencedTopDUContext context = parseCode(code, QLatin1String("test.zig"));


    DUChainReadLocker lock;
    DUContext *internalContext = getInternalContext(context, contextName);
    QVERIFY(internalContext);

    QVERIFY(context.data());
    qDebug() << "Decls are:";
    for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
        qDebug() << " - name" << decl->identifier();
        if (decl->abstractType()) {
            qDebug() << "   type" << decl->abstractType()->toString();
        }
    }
    qDebug() << "Problems are:";
    for (const auto &p : context->problems()) {
        qDebug() << p->description();
    }
    QCOMPARE(context->problems().size(), problems.size());
    for (const auto &problem: problems) {
        bool found = false;
        qDebug() << "Checking problem" << problem;
        for (const auto &p : context->problems()) {
            if (p->description().contains(problem)) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

}

void DUChainTest::testProblems_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QStringList>("problems");
    QTest::addColumn<QString>("contextName");
    QTest::newRow("use undefined") << "const x = y;" << QStringList{QLatin1String("Undefined variable y")} << "";
    QTest::newRow("invalid deref") << "const x = 1; const y = x.*;" << QStringList{QLatin1String("Attempt to dereference non-pointer type")} << "";
    QTest::newRow("invalid unwrap") << "const x = 1; const y = x.?;" << QStringList{QLatin1String("Attempt to unwrap non-optional type")} << "";
    QTest::newRow("invalid builtin") << "const x = @intToFloat(f32, 1);" << QStringList{QLatin1String("Undefined builtin")} << "";
    QTest::newRow("if bool") << "test{ if (1 > 2) {}\n}" << QStringList{} << "";
    QTest::newRow("if bool or") << "test{ if (false or 1 > 2) {}\n}" << QStringList{} << "";
    QTest::newRow("if bool and") << "test{ if (true and 1 > 2) {}\n}" << QStringList{} << "";
    QTest::newRow("if not bool") << "test{ if (1) {}\n}" << QStringList{QLatin1String("if condition is not a bool")} << "";
    QTest::newRow("if no capture") << "test{ var x: ?u8 = 0; if (x) {}\n}" << QStringList{QLatin1String("optional type with no capture")} << "";
    QTest::newRow("if capture non-optional") << "test{ var x: u8 = 0; if (x) |y| {}\n}" << QStringList{QLatin1String("Attempt to unwrap non-optional type")} << "";
    QTest::newRow("invalid array access") << "const x: u8 = 0; const y = x[1];" << QStringList{QLatin1String("Attempt to index non-array type")} << "";
    QTest::newRow("invalid array index") << "const x = [2]u8{1, 2}; const y = x[\"a\"];" << QStringList{QLatin1String("Array index is not an integer type")} << "";
    QTest::newRow("const str ptr index") << "const x = \"abcd\"; const y = x[0];" << QStringList{} << "";
    QTest::newRow("c ptr array access") << "const ptr: [*]const f32 = undefined; const x = ptr[0];" << QStringList{} << "";
    QTest::newRow("invalid array index var") << "const x = [2]u8{1, 2}; const i = 1.1; const y = x[i];" << QStringList{QLatin1String("Array index is not an integer type")} << "";
    QTest::newRow("enum") << "const Status = enum{Ok, Error}; const x: Status = .Ok;" << QStringList{} << "";
    QTest::newRow("invalid enum") << "const Status = enum{Ok, Error}; const x: Status = .Invalid;" << QStringList{QLatin1String("Invalid enum field Invalid")} << "";
    QTest::newRow("invalid enum access") << "const x: u8 = .Missing;" << QStringList{QLatin1String("Attempted to access enum field on non-enum type")} << "";
    QTest::newRow("enum assign") << "const Status = enum{Ok, Error}; test { var x = Status.Ok; x = .Error; \n}" << QStringList{} << "1,0";
    QTest::newRow("enum assign invalid ") << "const Status = enum{Ok, Error}; test { var x = Status.Ok; x = .Invalid; }" << QStringList{QLatin1String("Invalid enum field Invalid")} << "";
    QTest::newRow("enum switch case") << "const Status = enum{Ok, Error}; test { var x = Status.Ok; switch (x) { .Ok => {}, .Error => {}}}" << QStringList{} << "";
    QTest::newRow("enum switch case 2") << "const Status = enum{Ok, Error}; test { var x = Status.Ok; switch (x) { .Ok, .Error => {}}}" << QStringList{} << "";
    QTest::newRow("enum switch case 3") << "const Status = enum{Ok, Error}; test { var x = Status.Ok; switch (x) { .Ok, .Error, .Bad => {}}}" << QStringList{QLatin1String("Invalid enum field Bad")} << "";
    QTest::newRow("invalid enum switch case") << "const Status = enum{Ok, Error}; test { var x = Status.Ok; switch (x) { .Ok => {}, .Error => {}, .Invalid => {}}}" << QStringList{QLatin1String("Invalid enum field Invalid")} << "";
    QTest::newRow("enum array init") << "const Status = enum{Ok, Error}; var all = [_]Status{.Ok, .Error};" << QStringList{} << "";
    QTest::newRow("enum array init invalid") << "const Status = enum{Ok, Error}; var all = [_]Status{.Ok, .Error, .Invalid};" << QStringList{QLatin1String("Invalid enum field Invalid")} << "";
    QTest::newRow("enum alias") << "const Status = enum{Ok, Error}; const MyStatus = Status; test{ var x = MyStatus.Ok; }" << QStringList{} << "";
    QTest::newRow("enum comp") << "const Status = enum{Ok, Error}; test{ var x: Status = .Ok; if (x == .Ok) {}};" << QStringList{} << "";
    QTest::newRow("enum comp invalid") << "const Status = enum{Ok, Error}; test{ var x: Status = .Ok; if (x == .Bad) {}}" << QStringList{QLatin1String("Invalid enum field Bad")} << "";
    QTest::newRow("union enum switch") << "const Payload = union(enum){int: i32, float: f32}; test{ var x = Payload{.int=1}; switch (x) {.int => {}, .float=>{}} }" << QStringList{} << "";
    QTest::newRow("union enum switch invalid") << "const Payload = union(enum){int: i32, float: f32}; test{ var x = Payload{.int=1}; switch (x) {.int => {}, .float=>{}, .bool=>{}} }" << QStringList{QLatin1String("Invalid enum field bool")} << "";

    QTest::newRow("use out of order in mod") << "const x = y; const y = 1;" << QStringList{} << "";
    QTest::newRow("use out of order in struct") << "const Foo = struct {const x = y; const y = 1;};" << QStringList{} << "";
    QTest::newRow("use out of order in fn") << "pub fn foo() void {const x = y; const y = 1;}" << QStringList{QLatin1String("Undefined variable y")} << "";
    QTest::newRow("use out of order in fn in struct") << "const Foo = struct {pub fn foo() void {const x = y; const y = 1;}};" << QStringList{QLatin1String("Undefined variable y")} << "";

    // QTest::newRow("fn call undefined") << "test {var y = foo(); }" << QStringList{QLatin1String("Undefined function", "Undefined variable foo")} << "";
    QTest::newRow("fn call builtin") << "pub fn foo(x: u8) u8 {return x;} test {var y = foo(0); }" << QStringList{} << "";
    QTest::newRow("fn call missing arg") << "pub fn foo(x: u8) u8 {return x;} test {var y = foo(); }" << QStringList{QLatin1String("Expected 1 argument")} << "";
    QTest::newRow("fn call missing args") << "pub fn foo(x: u8, y: u8) u8 {return x + y;} test {var y = foo(); }" << QStringList{QLatin1String("Expected 2 arguments")} << "";
    QTest::newRow("fn call extra arg") << "pub fn foo(x: u8, y: u8) u8 {return x + y;} test {var y = foo(1, 2, 3); }" << QStringList{QLatin1String("Function has an extra argument")} << "";
    QTest::newRow("fn call extra args") << "pub fn foo(x: u8, y: u8) u8 {return x + y;} test {var y = foo(1, 2, 3, 4); }" << QStringList{QLatin1String("Function has 2 extra arguments")} << "";
    QTest::newRow("fn call enum arg inferred") << "const Status = enum{Ok, Error}; pub fn foo(status: Status) void {_ = status;} test {var y = foo(.Ok); }" << QStringList{} << "";
    QTest::newRow("fn call enum arg") << "const Status = enum{Ok, Error}; pub fn foo(status: Status) void {_ = status;} test {var y = foo(Status.Ok); }" << QStringList{} << "";
    QTest::newRow("fn call enum arg inferred invalid") << "const Status = enum{Ok, Error}; pub fn foo(status: Status) void {_ = status;} test {var y = foo(.Missing); }" << QStringList{QLatin1String("Invalid enum field Missing")} << "";
    QTest::newRow("fn call arg if inferred") << "const Status = enum{Ok, Error}; pub fn foo(status: Status) void {_ = status;} test {var x: bool = undefined; var y = foo(if (x) .Ok else .Error); }" << QStringList{} << "";
    QTest::newRow("fn call mismatch") << "pub fn foo(x: u8) u8 {return x;} test {var y = foo(true); }" << QStringList{QLatin1String("Argument 1 type mismatch. Expected u8 got bool")} << "";
    QTest::newRow("fn call implicit cast") << "pub fn foo(x: u16) u16 {return x;} test {const x: u8 = 1; var y = foo(x); }" << QStringList{} << "";
    QTest::newRow("fn call needs cast") << "pub fn foo(x: u16) u16 {return x;} test {const x: u32 = 1; var y = foo(x); }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("fn call slice") << "pub fn foo(x: []u8) void {} test {var x: [2]u8 = undefined; var y = foo(&x); }" << QStringList{} << "";
    QTest::newRow("extern fn call") << "extern fn add_char(a: u8, b: u8) u8; pub const add = add_char; test {var r = add(1, 2); }" << QStringList{} << "";

    // QTest::newRow("fn call slice 2") << "pub fn foo(x: []u8) void {} test {const x = [2]u8{1, 2}; var y = foo(&x); }" << QStringList{} << "";
    QTest::newRow("struct fn call") << "const Foo = struct {pub fn foo(self: Foo) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{} << "";
    QTest::newRow("struct fn call ptr") << "const Foo = struct {pub fn foo(self: *Foo) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{} << "";
    QTest::newRow("struct fn call arg") << "const Foo = struct {pub fn foo(self: Foo, other: u8) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{QLatin1String("Expected 1 argument")} << "";
    QTest::newRow("struct fn call ptr arg") << "const Foo = struct {pub fn foo(self: *Foo, other: u8) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{QLatin1String("Expected 1 argument")} << "";
    QTest::newRow("struct fn call not self") << "const Foo = struct {pub fn foo(other: u8) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{QLatin1String("Expected 1 argument")} << "";
    QTest::newRow("struct fn call self this") << "const Foo = struct {const Self = @This(); pub fn foo(self: Self) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{} << "";
    QTest::newRow("struct fn call self *this") << "const Foo = struct {const Self = @This(); pub fn foo(self: *Self) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{} << "";
    QTest::newRow("struct fn call self *this 2") << "const Foo = struct {const Self = @This(); pub fn foo(self: *Self) void { self.bar(); } pub fn bar(self: Self) void {}}; test {var f = Foo{}; var y = f.foo(); }" << QStringList{} << "";

    QTest::newRow("fn opt null") << "pub fn foo(bar: ?u8) void {} test {const y = foo(null); }" << QStringList{} << "";
    QTest::newRow("fn non-opt null") << "pub fn foo(bar: u8) void {} test {const y = foo(null); }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("fn opt base") << "pub fn foo(bar: ?u8) void {} test {var x: ?u8 = 0; const y = foo(x); }" << QStringList{} << "";
    QTest::newRow("fn opt comptime") << "pub fn foo(bar: ?u8) void {} test {const y = foo(1); }" << QStringList{} << "";
    QTest::newRow("fn opt base wrong") << "pub fn foo(bar: ?u8) void {} test {var x: ?i8 = 0; const y = foo(x); }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("fn type arg") << "pub fn foo(comptime T: type) void {} test {const y = foo(u8); }" << QStringList{} << "";
    // QTest::newRow("fn type arg null") << "pub fn foo(comptime T: type) void {} test {const y = foo(null); }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("fn type arg 2") << "pub fn foo(comptime T: type, bar: []const T) void {} test {const y = foo(u8, \"abcd\"); }" << QStringList{} << "";
    QTest::newRow("fn type arg 3") << "pub fn foo(comptime T: type, bar: []const T) void {} test {const y = foo(u16, \"abcd\"); }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("fn type arg 4") << "pub fn foo(comptime T: type, bar: []const T) []const T { return bar; } test {const x = foo(u8, \"abcd\"); const y = foo(u8, x); \n}" << QStringList{} << "1,0";
    QTest::newRow("fn arg []const u8") << "pub fn foo(bar: []const u8) void {} test {const y = foo(\"abcd\"); }" << QStringList{} << "";
    QTest::newRow("fn arg [:0]const u8") << "pub fn foo(bar: [:0]const u8) void {} test {const y = foo(\"abcd\"); }" << QStringList{} << "";
    QTest::newRow("fn arg []u8") << "pub fn foo(bar: []u8) void {} test {const y = foo(\"abcd\"); }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("fn arg slice to const") << "pub fn write(msg: []const u8) void {} test{ var x: [2]u8 = undefined; write(x[0..]); }" << QStringList{} << "";
    QTest::newRow("fn arg ptr to const slice") << "pub fn write(msg: []const u8) void {} test{ var x: [2]u8 = undefined; write(&x); }" << QStringList{} << "";
    QTest::newRow("fn error ok") << "pub fn write(msg: []const u8) !void {} test{ try write(\"abcd\"); }" << QStringList{} << "";
    QTest::newRow("fn error ok 2") << "pub fn write(msg: []const u8) !void {} test{ const r = write(\"abcd\"); }" << QStringList{} << "";
    QTest::newRow("fn error ok 3") << "pub fn write(msg: []const u8) !void {} test{ write(\"abcd\") catch {}; }" << QStringList{} << "";
    QTest::newRow("fn error ignored") << "pub fn write(msg: []const u8) !void {} test{ write(\"abcd\"); }" << QStringList{QLatin1String("Error is ignored")} << "";
    QTest::newRow("fn return ignored") << "pub fn write(msg: []const u8) usize { return 0; } test{ write(\"abcd\"); }" << QStringList{QLatin1String("Return value is ignored")} << "";
    QTest::newRow("fn noreturn ignored") << "pub fn exit(status: u8) noreturn { while (true) {}; } test{ exit(1); }" << QStringList{} << "";
    QTest::newRow("fn catch incompatible") << "pub fn write(msg: []const u8) !usize { return 0; } test{ write(\"abcd\") catch {}; }" << QStringList{QLatin1String("Incompatible types")} << "";
    QTest::newRow("fn catch 2") << "pub fn bufPrintZ(buf: []u8) ![:0]u8 { buf[buf.len-1] = 0; return buf; } test{ var buf: [100]u8 = undefined; const result = bufPrintZ(&buf) catch \"123\"; }" << QStringList{} << "";

    QTest::newRow("catch return") << "pub fn write(msg: []const u8) !usize { return 0; } pub fn main() void { const x = write(\"abcd\") catch { return; }; }" << QStringList{} << "";
    QTest::newRow("catch return 2") << "pub fn write(msg: []const u8) !usize { return 0; } pub fn main() void { const x = write(\"abcd\") catch |err| { if (true) { return; } return err; }; }" << QStringList{} << "";
    QTest::newRow("catch no return") << "pub fn write(msg: []const u8) !usize { return 0; } pub fn main() void { const x = write(\"abcd\") catch { }; }" << QStringList{QLatin1String("Incompatible types")} << "";
    QTest::newRow("catch panic") << "pub fn write(msg: []const u8) !usize { return 0; } pub fn main() void { const x = write(\"abcd\") catch @panic(\"doom\"); }" << QStringList{} << "";


    QTest::newRow("struct field") << "const A = struct {a: u8}; test {const x = A{.a = 0}; }" << QStringList{} << "";
    QTest::newRow("struct field type invalid") << "const A = struct {a: u8}; test {const x = A{.a = false}; }" << QStringList{QLatin1String("type mismatch")} << "";
    QTest::newRow("struct field name invalid") << "const A = struct {a: u8}; test {const x = A{.b = 0}; }" << QStringList{QLatin1String("Struct A has no field b")} << "";
    QTest::newRow("struct field enum") << "const S = enum {Ok, Err}; const A = struct {s: S}; test {const x = A{.s = .Ok}; }" << QStringList{} << "";
    QTest::newRow("struct field enum invalid") << "const S = enum {Ok, Err}; const A = struct {s: S}; test {const x = A{.s = .NotOk}; }" << QStringList{QLatin1String("Invalid enum field")} << "";
    QTest::newRow("struct field default enum ") << "const S = enum {Ok, Err}; const A = struct {s: S = .Ok};" << QStringList{} << "";
    QTest::newRow("struct field default enum invalid") << "const S = enum {Ok, Err}; const A = struct {s: S = .NotOk};" << QStringList{QLatin1String("Invalid enum field NotOk")} << "";
    QTest::newRow("struct field opt enum") << "const S = enum {Ok, Err}; const A = struct {s: ?S = null}; test { var a = A{.a = .Ok}; }" << QStringList{} << "";
    QTest::newRow("struct field inferred 1") << "const A = struct {a: f32}; test {const x: u8 = 1; const y = A{.a = @floatFromInt(x)}; }" << QStringList{} << "";
    //QTest::newRow("struct field inferred 2") << "const A = struct {a: f32}; test {const x = A{.a = @floatFromInt(1.1)}; }" << QStringList{} << "";
    QTest::newRow("struct field shadowed") << "const a = struct{ b: bool}; const Foo = struct {\n a: ?a.b\n};" << QStringList{} << "";

    QTest::newRow("struct dot init") << "const A = struct {a: u8}; pub fn foo(a: A) void {} test {const x = foo(.{.a = 0}); }" << QStringList{} << "";
    QTest::newRow("struct dot init invalid") << "const A = struct {a: u8}; pub fn foo(a: A) void {} test {const x = foo(.{.b = 0}); }" << QStringList{QLatin1String("Struct A has no field b")} << "";
    QTest::newRow("array init") << "const a = [_]u8{1,2};" << QStringList{} << "";
    QTest::newRow("array init type invalid") << "const a = [_]u8{1,false};" << QStringList{QLatin1String("Array item type mismatch at index 1")} << "";
    QTest::newRow("array init struct dot init") << "const A = struct {a: u8}; const b = [_]A{.{.a=2}};" << QStringList{} << "";
    QTest::newRow("array init struct dot init error") << "const A = struct {a: u8}; const b = [_]A{.{.a=true}};" << QStringList{QLatin1String("Struct field type mismatch")} << "";

    QTest::newRow("@intFromFloat()") << "test{var x: f32 = 0; var y: u32 = @intFromFloat(x);\n}" <<  QStringList{} << "";
    QTest::newRow("@floatFromInt()") << "test{var x: i32 = 0; var y: f32 = @floatFromInt(x);\n}" <<  QStringList{} << "";
    QTest::newRow("@intCast()") << "test{var x: i32 = 0; var y: u32 = @intCast(x);\n}" <<  QStringList{} << "";
    //QTest::newRow("@intFromFloat()") << "test{var x: f32 = 0; var y: u32 = @intFromFloat(x);\n}" << "y" << QStringList{} << "";
    QTest::newRow("vector access") << "const Vec = @Vector(4, f32); const v: Vec = undefined; const x = v[0];" << QStringList{} << "";
    QTest::newRow("vector from array") << "const a: [4]f32 = undefined; test{ var v: @Vector(4, f32) = undefined; v = a; }" << QStringList{} << "";
    QTest::newRow("vector from array 2") << "const a: [3]f32 = undefined; test{ var v: @Vector(4, f32) = undefined; v = a; }" << QStringList{QLatin1String("Assignment type mismatch")} << "";
    QTest::newRow("vector splat") << "test { var x: @Vector(4, f32) = undefined; x = @splat(1); }" << QStringList{} << "";
}

} // end namespace zig
