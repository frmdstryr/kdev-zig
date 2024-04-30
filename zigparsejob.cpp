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

#include "zigparsejob.h"

#include <serialization/indexedstring.h>
#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/ilanguagecontroller.h>
#include <language/interfaces/ilanguagesupport.h>
#include <language/backgroundparser/backgroundparser.h>
#include <language/backgroundparser/urlparselock.h>
#include <language/editor/documentrange.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/duchaindumper.h>
#include <project/projectmodel.h>
#include <util/path.h>

#include <QReadLocker>

#include "duchain/parsesession.h"
#include "duchain/kdevzigastparser.h"
#include "duchain/declarationbuilder.h"
#include "duchain/usebuilder.h"

#include "ziglanguagesupport.h"
#include "zigdebug.h"
#include <helpers.h>

using namespace KDevelop;

namespace Zig
{

ParseJob::ParseJob(const IndexedString &url, ILanguageSupport *languageSupport)
    : KDevelop::ParseJob(url, languageSupport)
{

}

LanguageSupport *ParseJob::zig() const
{
    return static_cast<LanguageSupport *>(languageSupport());
}

ParseSessionData::Ptr ParseJob::findParseSessionData(const IndexedString &url)
{
    DUChainReadLocker lock;
    auto context = KDevelop::DUChainUtils::standardContextForUrl(url.toUrl());

    if (context) {
        return ParseSessionData::Ptr(dynamic_cast<ParseSessionData *>(context->ast().data()));
    }

    return {};
}

ParseSessionData::Ptr ParseJob::createSessionData() const
{
    return ParseSessionData::Ptr(new ParseSessionData(document(), contents().contents, this, parsePriority()));
}


void ParseJob::run(ThreadWeaver::JobPointer self, ThreadWeaver::Thread *thread)
{
    Q_UNUSED(self);
    Q_UNUSED(thread);
    QReadLocker parseLock(languageSupport()->parseLock());
    if (abortRequested()) {
        return;
    }

    qCDebug(KDEV_ZIG) << "Parse job starting for: " << document().toUrl();
    {
        UrlParseLock urlLock(document());
        if (abortRequested() || !isUpdateRequired(ParseSession::languageString())) {
            return;
        }
        ProblemPointer readProblem = readContents();
        if (readProblem) {
            return;
        }
    }

    ParseSession session(findParseSessionData(document()));
    if (!session.data()) {
        session.setData(createSessionData());
    }
    session.parse();

    if (abortRequested()) {
        return;
    }

    ReferencedTopDUContext toUpdate = nullptr;
    {
        DUChainReadLocker lock;
        toUpdate = DUChainUtils::standardContextForUrl(document().toUrl());
    }
    if (toUpdate) {
        translateDUChainToRevision(toUpdate);
        DUChainWriteLocker lock; // Must come after translateDUChainToRevision
        toUpdate->setRange(RangeInRevision(0, 0, INT_MAX, INT_MAX));
        toUpdate->clearProblems();
    }

    if (abortRequested()) {
        return;
    }
    const auto num_errors = ast_error_count(session.ast());
    ReferencedTopDUContext context;
    if (num_errors == 0) {
        ZigNode root = {session.ast(), 0};
        qCDebug(KDEV_ZIG) << "Parsing succeeded for: " << document().toUrl();

        DeclarationBuilder builder;
        builder.setParseSession(&session);

        // For reasons I don't understand when using setAst the highlighting
        // breaks and the UpdateHighlighting hack used by the clangparsejob
        // must be used
        context = builder.build(document(), &root, toUpdate);
        setDuChain(context);

        if (abortRequested()) {
            return;
        }
        UseBuilder uses(document());
        uses.setParseSession(&session);
        uses.buildUses(&root);
    } else {
        qCDebug(KDEV_ZIG) << "Parsing failed for: " << document().toUrl();

        DUChainWriteLocker lock;
        context = toUpdate.data();

        if (context) {
            ParsingEnvironmentFilePointer parsingEnvironmentFile = context->parsingEnvironmentFile();
            parsingEnvironmentFile->setModificationRevision(contents().modification);
            context->clearProblems();
        } else {
            ParsingEnvironmentFile *file = new ParsingEnvironmentFile(document());
            file->setLanguage(ParseSession::languageString());

            context = new TopDUContext(document(), RangeInRevision(0, 0, INT_MAX, INT_MAX), file);
            DUChain::self()->addDocumentChain(context);
        }

        setDuChain(context);
    }

    if (abortRequested()) {
        return;
    }

    if (num_errors > 0) {
        DUChainWriteLocker lock;
        for (uint32_t i=0; i < num_errors; i++) {
            ZigError error = ZigError(ast_error_at(session.ast(), i));
            if(error.data() != nullptr) {
                ProblemPointer p = ProblemPointer(new Problem());
                p->setFinalLocation(DocumentRange(document(), KTextEditor::Range(
                    error.data()->range.start.line,
                    error.data()->range.start.column,
                    error.data()->range.end.line,
                    error.data()->range.end.column)));
                p->setSource(IProblem::Parser);
                p->setSeverity(static_cast<IProblem::Severity>(error.data()->severity));
                p->setDescription(QString::fromUtf8(error.data()->message));
                context->addProblem(p);
            }
        }
    }

    {
        DUChainWriteLocker lock;
        context->setFeatures(minimumFeatures());
        ParsingEnvironmentFilePointer file = context->parsingEnvironmentFile();
        Q_ASSERT(file);
        file->setModificationRevision(contents().modification);
        DUChain::self()->updateContextEnvironment(context->topContext(), file.data());
    }

    highlightDUChain();
    DUChain::self()->emitUpdateReady(document(), duChain());
    qCDebug(KDEV_ZIG) << "Parse job finished for: " << document().toUrl();
}

}
