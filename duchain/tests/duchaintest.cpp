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

#include "parsesession.h"
#include "declarationbuilder.h"
#include "usebuilder.h"

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


ReferencedTopDUContext parseCode(QString code)
{
    using namespace Zig;

    IndexedString document("temp");
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


DUContext *getInternalContext(ReferencedTopDUContext topContext, QString name)
{
    if (!topContext) {
        return nullptr;
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
    auto n = internalContext->childContexts().size();
    if (n != 1 || !internalContext->childContexts().first()) {
        if (n < 1) {
            qDebug() << "No child contexts for " << name;
        } else if (n > 1) {
            qDebug() << "Multiple child contexts for " << name;
        }
        return nullptr;
    }

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
    ReferencedTopDUContext context = parseCode(code);
    QVERIFY(context.data());

    DUChainReadLocker lock;
    auto decls = context->localDeclarations();
    QCOMPARE(decls.size(), 1);
    Declaration *funcDeclaration = decls.first();
    QVERIFY(funcDeclaration);
    QCOMPARE(funcDeclaration->identifier().toString(), QString("main"));
}

void DUChainTest::sanityCheckVar()
{
    QString code("const X = 1;");
    ReferencedTopDUContext context = parseCode(code);
    QVERIFY(context.data());

    DUChainReadLocker lock;
    auto decls = context->localDeclarations();
    QCOMPARE(decls.size(), 1);
    Declaration *varDeclaration = decls.first();
    QVERIFY(varDeclaration);
    QCOMPARE(varDeclaration->identifier().toString(), QString("X"));
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
    ReferencedTopDUContext context = parseCode(code);
    QVERIFY(context.data());

    DUChainReadLocker lock;
    DUContext *internalContext = contextName.isEmpty() ? context : getInternalContext(context, contextName);
    QVERIFY(internalContext);

    qDebug() << "Decls are:";
    for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
        qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
    }

    QCOMPARE(internalContext->localDeclarations().size(), bindings.size());
    for (const QString &binding : bindings) {
        qDebug() << "Checking name " << binding;
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
    QTest::newRow("fn var") << "main" << "pub fn main() void {\n  var y: u8 = 2;\n _ = y;\n}" << QStringList { "y" };
    QTest::newRow("struct decl") << "" << "const Foo = struct {};" << QStringList { "Foo" };
    QTest::newRow("fn multiple vars") << "main" << "pub fn main() void {\n var y: u8 = 2;\n var x = y; _ = x;\n}" << QStringList { "y", "x" };
    QTest::newRow("struct fn") << "Foo" << "const Foo = struct { pub fn bar() void {}};" << QStringList { "bar" };
    QTest::newRow("struct var") << "Foo" << "const Foo = struct { var x = 1; };" << QStringList { "x" };
    QTest::newRow("struct vars") << "Foo" << "const Foo = struct { var x = 1; const y: u8 = 0;};" << QStringList { "x", "y" };
    QTest::newRow("struct field") << "Foo" << "const Foo = struct { a: u8, };" << QStringList { "a" };
    QTest::newRow("test decl") << "" << "test \"foo\" {  }" << QStringList { "foo" };
    // Interal context ?
    // QTest::newRow("fn var in if") << "main" << "pub fn main() void { if (true) { var i: u8 = 0;} }" << QStringList { "i" };

}

void DUChainTest::testVarUsage()
{
    QFETCH(QString, code);
    QFETCH(CursorInRevision, cursor);
    QFETCH(QStringList, uses);
    qDebug() << "Code:" << code;
    ReferencedTopDUContext context = parseCode(code);
    QVERIFY(context.data());

    DUChainReadLocker lock;
    uint32_t i = 0;
    for (const QString &param : uses) {
        QStringList parts = param.split("@");
        QString use = parts[0];
        RangeInRevision expectedRange = rangeFromString(parts[1]);
        qDebug() << "Checking for use of" << use;
        auto localContext = context->findContextAt(cursor);
        //qDebug() << "Local decls at:" << cursor;

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
        << "const Foo = struct{};\ntest {\n var x = Foo{};\n}" << QStringList { "Foo@2,9,2,12" };
    QTest::newRow("fn call") << CursorInRevision(3, 0)
        << "pub fn main() void {}\ntest {\n main();\n}" << QStringList { "main@2,1,2,5" };

}
