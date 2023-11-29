/*
    SPDX-FileCopyrightText: 2012 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "completiontest.h"

#include <language/backgroundparser/backgroundparser.h>
#include <language/codecompletion/codecompletiontesthelper.h>
#include <language/duchain/declaration.h>
#include <language/codegen/coderepresentation.h>
#include <language/duchain/duchain.h>
#include <interfaces/ilanguagecontroller.h>

#include <tests/testcore.h>
#include <tests/autotestshell.h>
#include <tests/testlanguagecontroller.h>

#include <ktexteditor_version.h>
#include <KTextEditor/Editor>
#include <KService>

#include "context.h"

#include <QDebug>
#include <QStandardPaths>
#include <QTest>

using namespace KDevelop;

QTEST_MAIN(Zig::CompletionTest)

static int testId = 0;
static QString basepath = "/tmp/kdevzigcompletiontest.dir/";

namespace Zig {

QStandardItemModel& fakeModel() {
  static QStandardItemModel model;
  model.setColumnCount(10);
  model.setRowCount(10);
  return model;
}

QString filenameForTestId(const int id) {
    return QString("%1test_%2.zig").arg(basepath).arg(id);
}

QString nextFilename() {
    testId += 1;
    return filenameForTestId(testId);
}

CompletionTest::CompletionTest(QObject* parent) : QObject(parent)
{
    initShell();
}

ReferencedTopDUContext makeAndParseFile(QString filename, QString contents) {
    QString path = QDir::cleanPath(filename);
    QFile fileptr;
    fileptr.setFileName(path);
    fileptr.open(QIODevice::WriteOnly);
    fileptr.write(contents.toUtf8());
    fileptr.close();
    auto url = QUrl::fromLocalFile(path);
    qCDebug(KDEV_ZIG) <<  "updating duchain for " << url.toString();
    const IndexedString urlstring(url);
    return DUChain::self()->waitForUpdate(urlstring, KDevelop::TopDUContext::ForceUpdate);
}

void CompletionTest::initShell()
{
    QDir d;
    d.mkpath(basepath);
    AutoTestShell::init();
    TestCore* core = new TestCore();
    core->initialize(KDevelop::Core::NoUi);

    DUChain::self()->disablePersistentStorage();
    KDevelop::CodeRepresentation::setDiskChangesForbidden(true);

    // This seems to be required for the waitForUpdate to work in tests... ?
    auto* langController = new TestLanguageController(core);
    core->setLanguageController(langController);
    langController->backgroundParser()->setThreadCount(4);
    langController->backgroundParser()->abortAllJobs();
    m_langSupport = new LanguageSupport(core);
    langController->addTestLanguage(m_langSupport, QStringList() << QStringLiteral("text/plain"));
    const auto languages = langController->languagesForUrl(QUrl::fromLocalFile(QStringLiteral("/foo.zig")));
    QCOMPARE(languages.size(), 1);
    QCOMPARE(languages.first(), m_langSupport);

}

const QList<CompletionTreeItem*> CompletionTest::invokeCompletionOn(const QString& initCode, const QString& invokeCode)
{
    CompletionParameters data = prepareCompletion(initCode, invokeCode);
    return runCompletion(data);
}

const CompletionParameters CompletionTest::prepareCompletion(const QString& initCode, const QString& invokeCode)
{
    CompletionParameters completion_data;

    QString filename = nextFilename();
    Q_ASSERT(initCode.indexOf("%INVOKE") != -1);

    // Parse file before completion line
    QStringList oldLines;
    for (const auto &line: initCode.split('\n')) {
        if (line.contains("%INVOKE"))
            continue;
        oldLines.append(line);
    }
    ReferencedTopDUContext topContext = makeAndParseFile(
        filename,
        oldLines.join('\n')
    );
    Q_ASSERT(topContext);

    QString copy = initCode;

    QString allCode = copy.replace("%INVOKE", invokeCode);

    QStringList lines = allCode.split('\n');
    completion_data.cursorAt = CursorInRevision::invalid();
    for ( int i = 0; i < lines.length(); i++ ) {
        int j = lines.at(i).indexOf("%CURSOR");
        if ( j != -1 ) {
            completion_data.cursorAt = CursorInRevision(i, j);
            break;
        }
    }
    Q_ASSERT(completion_data.cursorAt.isValid());

    // codeCompletionContext only gets passed the text until the place where completion is invoked
    completion_data.snip = allCode.mid(0, allCode.indexOf("%CURSOR"));
    completion_data.remaining = allCode.mid(allCode.indexOf("%CURSOR") + 7);

    DUChainReadLocker lock;
    completion_data.contextAtCursor = DUContextPointer(topContext->findContextAt(completion_data.cursorAt, true));
    Q_ASSERT(completion_data.contextAtCursor);

    return completion_data;
}

const QList<CompletionTreeItem*> CompletionTest::runCompletion(const CompletionParameters parameters)
{
    CompletionContext* context = new CompletionContext(
        parameters.contextAtCursor,
        parameters.snip,
        parameters.remaining,
        parameters.cursorAt
    );
    bool abort = false;
    QList<CompletionTreeItem*> items;
    foreach ( CompletionTreeItemPointer ptr, context->completionItems(abort, true) ) {
        items << ptr.data();
        // those are leaked, but it's only a few kb while the tests are running. who cares.
        m_ptrs << ptr;
    }
    return items;
}

bool CompletionTest::containsItemForDeclarationNamed(const QList<CompletionTreeItem*> items, QString itemName)
{
    foreach ( const CompletionTreeItem* ptr, items ) {
        if ( ptr->declaration() ) {
            if ( ptr->declaration()->identifier().toString() == itemName ) {
                return true;
            }
        }
    }
    return false;
}

bool CompletionTest::containsItemStartingWith(const QList<CompletionTreeItem*> items, const QString& itemName)
{
    QModelIndex idx = fakeModel().index(0, KDevelop::CodeCompletionModel::Name);
    foreach ( const CompletionTreeItem* ptr, items ) {
        if ( ptr->data(idx, Qt::DisplayRole, nullptr).toString().startsWith(itemName) ) {
            return true;
        }
    }
    return false;
}

bool CompletionTest::itemInCompletionList(const QString& initCode, const QString& invokeCode, QString itemName)
{
    QList< CompletionTreeItem* > items = invokeCompletionOn(initCode, invokeCode);
    return containsItemStartingWith(items, itemName);
}

bool CompletionTest::declarationInCompletionList(const QString& initCode, const QString& invokeCode, QString itemName)
{
    QList< CompletionTreeItem* > items = invokeCompletionOn(initCode, invokeCode);
    return containsItemForDeclarationNamed(items, itemName);
}

bool CompletionTest::completionListIsEmpty(const QString& initCode, const QString& invokeCode)
{
    return invokeCompletionOn(initCode, invokeCode).isEmpty();
}



void CompletionTest::testFieldAccess()
{
    QFETCH(QString, invokeCode);
    QFETCH(QString, completionCode);
    QFETCH(QString, expectedDeclaration);

    QVERIFY(declarationInCompletionList(invokeCode, completionCode, expectedDeclaration));
}

void CompletionTest::testFieldAccess_data()
{
    QTest::addColumn<QString>("invokeCode");
    QTest::addColumn<QString>("completionCode");
    QTest::addColumn<QString>("expectedDeclaration");

    QTest::newRow("struct")
        << "const A = struct {a: u8, b:u8};\n"
           "const x = A{};\n"
           "const y = x.%INVOKE" << "%CURSOR" << "a";
    QTest::newRow("nested struct")
        << "const C = struct {a: u8, b:u8};\n"
           "const B = struct {c: C};\n"
           "const A = struct {b: B };\n"
           "const x = A{};\n"
           "x.b.c.%INVOKE" << "%CURSOR" << "a";
    QTest::newRow("nested struct assign")
        << "const B = struct {a: u8, b:u8};\n"
           "const A = struct {b: B };\n"
           "const x = A{};\n"
           "const y = x.b.%INVOKE" << "%CURSOR" << "a";

}


} // Namespace zig

#include "moc_completiontest.cpp"
