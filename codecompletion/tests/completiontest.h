/*
    SPDX-FileCopyrightText: 2011-2012 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>

#include <language/editor/cursorinrevision.h>
#include <language/codecompletion/codecompletioncontext.h>

#include "model.h"
#include "completiondebug.h"
#include "../../ziglanguagesupport.h"

using namespace KDevelop;

namespace Zig {

struct CompletionParameters {
    DUContextPointer contextAtCursor;
    QString snip;
    QString remaining;
    CursorInRevision cursorAt;
};

class CompletionTest : public QObject
{
    Q_OBJECT
    public:
        explicit CompletionTest(QObject* parent = nullptr);
        void initShell();
        
        const QList<CompletionTreeItem*> invokeCompletionOn(const QString& initCode, const QString& invokeCode);
        const CompletionParameters prepareCompletion(const QString& initCode, const QString& invokeCode);
        const QList<CompletionTreeItem*> runCompletion(const CompletionParameters data);

        bool containsItemForDeclarationNamed(const QList< CompletionTreeItem* > items, QString itemName);
        // convenience function
        bool declarationInCompletionList(const QString& initCode, const QString& invokeCode, QString itemName);
        // convenience function
        bool completionListIsEmpty(const QString& initCode, const QString& invokeCode);
        // convenience function
        bool containsItemStartingWith(const QList< CompletionTreeItem* > items, const QString& itemName);
        // convenience function
        bool itemInCompletionList(const QString& initCode, const QString& invokeCode, QString itemName);
        
    private Q_SLOTS:
        void testFieldAccess();
        void testFieldAccess_data();

    private:
        QList<CompletionTreeItemPointer> m_ptrs;
        LanguageSupport* m_langSupport;

};

} // namespace zig
