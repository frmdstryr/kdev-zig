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

#include "ziglanguagesupport.h"

#include <zigdebug.h>

#include <interfaces/icore.h>
#include <interfaces/idocument.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/ilanguagecontroller.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/codecompletion/codecompletion.h>

#include <KPluginFactory>
#include <KPluginLoader>

#include <QReadWriteLock>
#include <QStandardPaths>

#include "zigparsejob.h"
#include "codecompletion/model.h"
#include "projectconfig/projectconfigpage.h"
#include <language/backgroundparser/backgroundparser.h>
#include <language/duchain/parsingenvironment.h>
#include "zigparsejob.h"

K_PLUGIN_FACTORY_WITH_JSON(KDevZigSupportFactory, "kdevzigsupport.json", registerPlugin<Zig::LanguageSupport>(); )

using namespace KDevelop;

namespace Zig
{

LanguageSupport* LanguageSupport::m_self = nullptr;

LanguageSupport::LanguageSupport(QObject *parent, const QVariantList &args)
    : KDevelop::IPlugin(QStringLiteral("kdevzigsupport"), parent),
      KDevelop::ILanguageSupport(),
      m_highlighting(new Highlighting(this))
{
    m_self = this;
    Q_UNUSED(args);

    new CodeCompletion(this, new CompletionModel(this), name());

    connect(ICore::self()->documentController(), &IDocumentController::documentActivated,
            this, &LanguageSupport::documentActivated);
}

LanguageSupport::~LanguageSupport()
{
    parseLock()->lockForWrite();
    parseLock()->unlock();

    delete m_highlighting;
    m_highlighting = nullptr;
}

QString LanguageSupport::name() const
{
    return "Zig";
}

KDevelop::ParseJob *LanguageSupport::createParseJob(const KDevelop::IndexedString &url)
{
    return new ParseJob(url, this);
}

ICodeHighlighting *LanguageSupport::codeHighlighting() const
{
    return m_highlighting;
}

SourceFormatterItemList LanguageSupport::sourceFormatterItems() const
{
    SourceFormatterStyle zigFormatter("zig fmt");
    zigFormatter.setCaption("zig fmt");
    zigFormatter.setDescription(i18n("Format source with zig fmt."));
    zigFormatter.setMimeTypes(SourceFormatterStyle::MimeList {
        SourceFormatterStyle::MimeHighlightPair { "text/zig", "Zig" },
        SourceFormatterStyle::MimeHighlightPair { "text/x-zig", "Zig" }
    });

    QString zigPath = QStandardPaths::findExecutable("zig");
    if (zigPath.isEmpty()) {
        qCDebug(KDEV_ZIG) << "Could not find the zig executable";
        zigPath = "/usr/bin/zig";
    }
    zigFormatter.setContent(zigPath + " fmt $TMPFILE");

    return SourceFormatterItemList { SourceFormatterStyleItem { "customscript", zigFormatter } };
}

int LanguageSupport::perProjectConfigPages() const
{
    return 1;
}

KDevelop::ConfigPage* LanguageSupport::perProjectConfigPage(int number, const KDevelop::ProjectConfigOptions& options, QWidget* parent)
{
    if ( number == 0 ) {
        return new ProjectConfigPage(this, options, parent);
    }
    return nullptr;
}

void LanguageSupport::documentActivated(IDocument* doc)
{
    TopDUContext::Features features;
    {
        DUChainReadLocker lock;
        auto ctx = DUChainUtils::standardContextForUrl(doc->url());
        if (!ctx) {
            return;
        }

        auto file = ctx->parsingEnvironmentFile();
        if (!file) {
            return;
        }

        if (file->language() != ParseSession::languageString()) {
            return;
        }

        if (file->needsUpdate()) {
            return;
        }

        features = ctx->features();
    }

    const auto indexedUrl = IndexedString(doc->url());

    auto sessionData = ParseJob::findParseSessionData(indexedUrl);
    if (sessionData) {
        return;
    }

    if ((features & TopDUContext::AllDeclarationsContextsAndUses) != TopDUContext::AllDeclarationsContextsAndUses) {
        // the file was parsed in simplified mode, we need to reparse it to get all data
        // now that its opened in the editor
        features = TopDUContext::AllDeclarationsContextsAndUses;
    } else {
        features = static_cast<TopDUContext::Features>(ParseJob::AttachASTWithoutUpdating | features);
        if (ICore::self()->languageController()->backgroundParser()->isQueued(indexedUrl)) {
            // The document is already scheduled for parsing (happens when opening a project with an active document)
            // The background parser will optimize the previous request out, so we need to update highlighting
            features = static_cast<TopDUContext::Features>(ParseJob::UpdateHighlighting | features);
        }
    }
    ICore::self()->languageController()->backgroundParser()->addDocument(indexedUrl, features);
}


}

#include "ziglanguagesupport.moc"
