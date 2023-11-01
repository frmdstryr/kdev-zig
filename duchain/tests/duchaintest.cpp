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

#include <language/duchain/ducontext.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/topducontext.h>
#include <tests/testcore.h>
#include <tests/autotestshell.h>
#include <tests/testhelpers.h>

#include "types/builtintype.h"
#include "parsesession.h"
#include "declarationbuilder.h"
#include "usebuilder.h"
#include "helpers.h"

using namespace KDevelop;

QTEST_MAIN(DUChainTest)

// Parse start_line,start_col,end_line,end_col into range
static RangeInRevision rangeFromString(const QString &values) {
    auto parts = values.trimmed().split(",");
    Q_ASSERT(parts.size() == 4);
    CursorInRevision start(parts[0].trimmed().toInt(), parts[1].trimmed().toInt());
    CursorInRevision end(parts[2].trimmed().toInt(), parts[3].trimmed().toInt());
    RangeInRevision range(start, end);
    return range;
}

ReferencedTopDUContext parseCode(QString code, QString name)
{
    using namespace Zig;

    IndexedString document(name);
    ParseSessionData *sessionData = new ParseSessionData(document, code.toUtf8());
    ParseSession session(ParseSessionData::Ptr(nullptr));
    session.setData(ParseSessionData::Ptr(sessionData));
    session.parse();

    ZigNode root = {session.ast(), 0};
    DeclarationBuilder declarationBuilder;
    UseBuilder useBuilder(document);

    declarationBuilder.setParseSession(&session);
    useBuilder.setParseSession(&session);

    ReferencedTopDUContext context = declarationBuilder.build(document, &root);
    useBuilder.buildUses(&root);

    return context;
}

ReferencedTopDUContext parseStdCode(QString path)
{
    QString filename = Zig::Helper::stdLibPath(nullptr) + "/" + path;
    QFile f(filename);
    Q_ASSERT(f.open(QIODevice::ReadOnly));
    return parseCode(f.readAll(), filename);
}

DUContext *getInternalContext(ReferencedTopDUContext topContext, QString name, bool firstChildContext=false)
{
    if (!topContext) {
        return nullptr;
    }

    if (name.isEmpty()) {
        return topContext;
    }

    if (name.contains(",")) {
        auto parts = name.split(",");
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
    AutoTestShell::init();
    TestCore::initialize(Core::NoUi);
}

void DUChainTest::sanityCheckFn()
{
    QString code("fn main() !void {}");
    ReferencedTopDUContext context = parseCode(code, "test");
    QVERIFY(context.data());

    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier("main"));
    QCOMPARE(decls.size(), 1);
    Declaration *funcDeclaration = decls.first();
    QVERIFY(funcDeclaration);
    QCOMPARE(funcDeclaration->identifier().toString(), QString("main"));
    QCOMPARE(funcDeclaration->abstractType()->toString(), QString("function !void ()"));
}

void DUChainTest::sanityCheckVar()
{
    QString code("const X = 1;");
    ReferencedTopDUContext context = parseCode(code, "test");
    QVERIFY(context.data());

    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier("X"));
    QCOMPARE(decls.size(), 1);
    Declaration *varDeclaration = decls.first();
    QVERIFY(varDeclaration);
    QCOMPARE(varDeclaration->identifier().toString(), QString("X"));
    QCOMPARE(varDeclaration->abstractType()->toString(), QString("comptime_int"));
}

void DUChainTest::sanityCheckImport()
{
    ReferencedTopDUContext timecontext = parseStdCode("time.zig");
    ReferencedTopDUContext stdcontext = parseStdCode("std.zig");
    QString code("const std = @import(\"std\");const time = std.time; const ns_per_us = time.ns_per_us;");
    ReferencedTopDUContext context = parseCode(code, "test");
    QVERIFY(context.data());
    DUChainReadLocker lock;
    auto decls = context->findDeclarations(Identifier("std"));
    QCOMPARE(decls.size(), 1);
    //QVERIFY(decls.first()->abstractType()->modifiers() & Zig::ModuleModifier); // std
    decls = context->findDeclarations(Identifier("time"));
    //QVERIFY(decls.first()->abstractType()->modifiers() & Zig::ModuleModifier); // time
    QCOMPARE(decls.size(), 1);
    decls = context->findDeclarations(Identifier("ns_per_us"));
    QCOMPARE(decls.size(), 1);
    Declaration *decl = decls.first();
    QVERIFY(decl);
    QCOMPARE(decl->identifier().toString(), QString("ns_per_us"));
    QCOMPARE(decl->abstractType()->toString(), QString("comptime_int"));
}


void DUChainTest::cleanupTestCase()
{
    TestCore::shutdown();
}

void DUChainTest::testVarBindings()
{
    QFETCH(QString, code);
    QFETCH(QString, contextName);
    QFETCH(QStringList, bindings);
    qDebug() << "Code:" << code;
    ReferencedTopDUContext context = parseCode(code, "test");
    QVERIFY(context.data());

    DUChainReadLocker lock;
    DUContext *internalContext = getInternalContext(context, contextName);
    QVERIFY(internalContext);

    qDebug() << "Decls are:";
    for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    for (const QString &binding : bindings) {
        QCOMPARE(internalContext->findLocalDeclarations(Identifier(binding)).size(),  1);
    }
}

void DUChainTest::testVarBindings_data()
{
    QTest::addColumn<QString>("contextName");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QStringList>("bindings");

    QTest::newRow("simple var") << "" << "var x = 1;" << QStringList { "x" };
    QTest::newRow("simple const") << "" << "const y = 2;" << QStringList { "y" };
    QTest::newRow("simple var typed") << "" << "var y: u8 = 2;" << QStringList { "y" };
    QTest::newRow("multiple vars") << "" << "var x = 1;\nvar y = 2;" << QStringList { "x", "y" };
    QTest::newRow("fn and var") << "" << "var x = 1;\npub fn foo() void {}" << QStringList { "x", "foo" };
    QTest::newRow("fn var") << "1,0" << "pub fn main() void {\n  var y: u8 = 2;\n _ = y;\n}" << QStringList { "y" };
    QTest::newRow("struct decl") << "" << "const Foo = struct {};" << QStringList { "Foo" };
    QTest::newRow("fn multiple vars") << "1,0" << "pub fn main() void {\n var y: u8 = 2;\n var x = y; _ = x;\n}" << QStringList { "y", "x" };
    QTest::newRow("struct fn") << "Foo" << "const Foo = struct { pub fn bar() void {}};" << QStringList { "bar" };
    QTest::newRow("struct var") << "Foo" << "const Foo = struct { var x = 1; };" << QStringList { "x" };
    QTest::newRow("struct vars") << "Foo" << "const Foo = struct { var x = 1; const y: u8 = 0;};" << QStringList { "x", "y" };
    QTest::newRow("struct field") << "Foo" << "const Foo = struct { a: u8, };" << QStringList { "a" };
    QTest::newRow("test decl") << "" << "test \"foo\" {  }" << QStringList { "foo" };
    QTest::newRow("if capture") << "2,0" << "test {var opt: ?u8 = null;\n if (opt) |x| {\n _ = x;\n} }" << QStringList { "x" };
    // Interal context ?
    // QTest::newRow("fn var in if") << "main" << "pub fn main() void { if (true) { var i: u8 = 0;} }" << QStringList { "i" };

}

void DUChainTest::testVarUsage()
{
    QFETCH(QString, code);
    QFETCH(CursorInRevision, cursor);
    QFETCH(QStringList, uses);
    qDebug() << "Code:" << code;
    ReferencedTopDUContext context = parseCode(code, "test");
    QVERIFY(context.data());

    DUChainReadLocker lock;
    uint32_t i = 0;
    for (const QString &param : uses) {
        QStringList parts = param.split("/");
        QString use = parts[0];
        RangeInRevision expectedRange = rangeFromString(parts[1]);
        qDebug() << "Checking for use of" << use;
        auto localContext = cursor.isValid() ? context->findContextAt(cursor) : context;
        //qDebug() << "Local decls at:" << cursor;
        qDebug() << "Locals are:";
        for (const KDevelop::Declaration *decl : localContext->localDeclarations()) {
            qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
        }

        auto decls = localContext->findDeclarations(Identifier(use));
        for (const KDevelop::Declaration *decl : decls) {
            qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
        }

        QVERIFY(decls.size() >  0);
        QCOMPARE(decls.first()->uses().size(),  1);
        QVector<RangeInRevision> useRanges = decls.first()->uses().values().at(0);
        QCOMPARE(useRanges.size(),  1);
        QCOMPARE(useRanges.at(0), expectedRange);

        i += 1;
    }
}

void DUChainTest::testVarUsage_data()
{
    QTest::addColumn<CursorInRevision>("cursor");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QStringList>("uses");

    QTest::newRow("struct init") << CursorInRevision(3, 0)
        << "const Foo = struct{};\ntest {\n var x = Foo{};\n}" << QStringList { "Foo/2,9,2,12" };
    QTest::newRow("fn call") << CursorInRevision(3, 0)
        << "pub fn main() void {}\ntest {\n main();\n}" << QStringList { "main/2,1,2,5" };
    QTest::newRow("fn arg") << CursorInRevision(2, 0)
        << "pub fn main(a: u8) void {\nconst b=1; return a;\n}" << QStringList { "a/1,8,1,9" };
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
    ReferencedTopDUContext context = parseCode(code, "test");
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
    QTest::newRow("const bool") << "const x = true;" << "x" << "bool" << "";
    QTest::newRow("var *u8") << "var x: *u8 = 0;" << "x" << "*u8" << "";
    QTest::newRow("var ?u8") << "var x: ?u8 = 0;" << "x" << "?u8" << "";
    QTest::newRow("var ?*u8") << "var x: ?*u8 = 0;" << "x" << "?*u8" << "";
    QTest::newRow("enum") << "const Day = enum{Mon, Tue};" << "Day" << "Day" << "";
    QTest::newRow("enum arg") << "const Day = enum(u8){Mon, Tue}; const mon = Day.Mon;" << "mon" << "u8" << "";
    QTest::newRow("struct") << "const Foo = struct {a: u8};" << "Foo" << "Foo" << "";
    QTest::newRow("struct field") << "const Foo = struct {\n a: u8\n};" << "a" << "u8" << "Foo";
    QTest::newRow("fn void") << "pub fn main() void {}" << "main" << "function void ()"<< "";
    QTest::newRow("fn !void") << "pub fn main() !void {}" << "main" << "function !void ()"<< "";
    QTest::newRow("fn u8") << "pub fn main() u8 {}" << "main" << "function u8 ()"<< "";
    QTest::newRow("fn arg") << "pub fn main(a: bool) void {}" << "main" << "function void (bool)"<< "";
    QTest::newRow("fn err!void") << "const WriteError = error{EndOfStream};\npub fn main() WriteError!void {}" << "main" << "function WriteError!void ()"<< "";
    QTest::newRow("var struct") << "const Foo = struct {a: u8};\ntest {\nvar f = Foo{};}" << "f" << "Foo" << "2,0";
    QTest::newRow("field access") << "const Foo = struct {a: u8=0};\ntest {\nvar f = Foo{}; var b = f.a;\n}" << "b" << "u8" << "3,0";
    QTest::newRow("field ptr") << "const Foo = struct {a: u8=0};\ntest {\nvar f = &Foo{}; var b = f.a;\n}" << "b" << "u8" << "3,0";
    QTest::newRow("fn call") << "pub fn foo() f32 { return 0.0; }\ntest {\nvar f = foo();\n}" << "f" << "f32" << "3,0";
    QTest::newRow("var addr of") << "const Foo = struct {a: u8};\ntest {\nvar f = Foo{}; var x = &f;\n}" << "x" << "*Foo" << "3,0";
    QTest::newRow("ptr deref") << "test {\nvar buf: *u8 = undefined; var x = buf.*;\n}" << "x" << "u8" << "2,0";
    QTest::newRow("unwrap optional") << "var x: ?u8 = 0; const y = x.?;" << "y" << "u8" << "";
    QTest::newRow("bool >") << "var x = 1 > 2;" << "x" << "bool" << "";
    QTest::newRow("bool >=") << "var x = 1 >= 2;" << "x" << "bool" << "";
    QTest::newRow("bool <") << "var x = 1 < 2;" << "x" << "bool" << "";
    QTest::newRow("bool <=") << "var x = 1 <= 2;" << "x" << "bool" << "";
    QTest::newRow("bool ==") << "var x = 1 == 2;" << "x" << "bool" << "";
    QTest::newRow("bool !=") << "var x = 1 != 2;" << "x" << "bool" << "";
    QTest::newRow("try") << "pub fn main() !f32 {return 1;}\ntest{\nvar x = try main(); \n}" << "x" << "f32"<< "3,0";
    QTest::newRow("var fixed array") << "var x: [2]u8 = undefined;" << "x" << "[2]u8" << "";
    QTest::newRow("array access") << "var x: [2]u8 = undefined; var y = x[0];" << "y" << "u8" << "";
    QTest::newRow("array len") << "var x: [2]u8 = undefined; var y = x.len;" << "y" << "usize" << "";
    QTest::newRow("array slice") << "var x: [2]u8 = undefined; var y = x[0..];" << "y" << "[]u8" << "";
    QTest::newRow("sentinel array") << "var x: [100:0]u8 = undefined;" << "x" << "[100:0]u8" << "";
    QTest::newRow("ptr type aligned") << "var x: []u8 = undefined;" << "x" << "[]u8" << "";
    QTest::newRow("ptr type aligned ptr") << "var x: *align(4)u8 = undefined;" << "x" << "*u8" << "";

    // Builtin calls
    QTest::newRow("@This()") << "const Foo = struct { const Self = @This();\n};" << "Self" << "Foo" << "1,0";
    QTest::newRow("@sizeOf()") << "const Foo = struct { a: u8, }; test {var x = @sizeOf(Foo);\n}" << "x" << "comptime_int" << "1,0";
    QTest::newRow("@as()") << "test{var x = @as(u8, 1);\n}" << "x" << "u8" << "1,0";
    QTest::newRow("@min()") << "test{var x: u32 = 1; var y = @min(x, 1);\n}" << "x" << "u32" << "1,0";
    // TODO: constness
    QTest::newRow("@tagName()") << "const Foo = enum{A, B}; test{var x = @tagName(Foo.A);\n}" << "x" << "[:0]const u8" << "1,0";

    QTest::newRow("nested struct") << "const Foo = struct {bar: Bar = .{}, const Bar = struct {a: u8};}; test{\nvar f = Foo.Bar{};\n}" << "f" << "Foo::Bar" << "2,0";
}
