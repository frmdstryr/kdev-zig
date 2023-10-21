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
#include <language/duchain/topducontext.h>
#include <tests/testcore.h>
#include <tests/autotestshell.h>
#include <tests/testhelpers.h>

#include "parsesession.h"
#include "declarationbuilder.h"
#include "usebuilder.h"

using namespace KDevelop;

QTEST_MAIN(DUChainTest)

ReferencedTopDUContext parseCode(QString code)
{
    using namespace Zig;

    IndexedString document("temp");
    ParseSessionData *sessionData = new ParseSessionData(document, code.toUtf8());
    ParseSession session(ParseSessionData::Ptr(nullptr));
    session.setData(ParseSessionData::Ptr(sessionData));
    session.parse();

    ZNode root = {session.ast(), 0};
    DeclarationBuilder declarationBuilder;
    UseBuilder useBuilder(document);

    declarationBuilder.setParseSession(&session);
    useBuilder.setParseSession(&session);

    DUChainWriteLocker lock;
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

    ReferencedTopDUContext context = parseCode(code);
    QVERIFY(context.data());

    DUChainReadLocker lock;
    if (!contextName.isEmpty()) {
        DUContext *internalContext = getInternalContext(context, contextName);
        QVERIFY(internalContext);
        QCOMPARE(internalContext->localDeclarations().size(), bindings.size());

        qDebug() << "Decls are:";
        for (const KDevelop::Declaration *decl : internalContext->localDeclarations()) {
            qDebug() << "  name" << decl->identifier() << " type" << decl->abstractType()->toString();
        }

        for (const QString &binding : bindings) {
            QCOMPARE(internalContext->findLocalDeclarations(Identifier(binding)).size(),  1);
        }
    } else {
        qDebug() << "Decls are:";
        for (const KDevelop::Declaration *decl : context->localDeclarations()) {
            qDebug() << "  " << decl->toString();
        }
        for (const QString &binding : bindings) {
            QCOMPARE(context->findLocalDeclarations(Identifier(binding)).size(),  1);
        }
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

}
