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
#include <language/codecompletion/codecompletion.h>

#include <KPluginFactory>
#include <KPluginLoader>

#include <QReadWriteLock>
#include <QStandardPaths>

#include "zigparsejob.h"
#include "codecompletion/model.h"

K_PLUGIN_FACTORY_WITH_JSON(KDevZigSupportFactory, "kdevzigsupport.json", registerPlugin<Zig::LanguageSupport>(); )

using namespace KDevelop;

namespace Zig
{

LanguageSupport::LanguageSupport(QObject *parent, const QVariantList &args)
    : KDevelop::IPlugin(QStringLiteral("kdevzigsupport"), parent),
      KDevelop::ILanguageSupport(),
      m_highlighting(new Highlighting(this))
{
    Q_UNUSED(args);

    new CodeCompletion(this, new CompletionModel(this), name());
}

LanguageSupport::~LanguageSupport()
{
    parseLock()->lockForWrite();
    parseLock()->unlock();

    delete m_highlighting;
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

}

#include "ziglanguagesupport.moc"
