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
    return QLatin1String("Zig");
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
    SourceFormatterStyle zigFormatter(QLatin1String("zig fmt"));
    zigFormatter.setCaption(QLatin1String("zig fmt"));
    zigFormatter.setDescription(i18n("Format source with zig fmt."));
    zigFormatter.setMimeTypes(SourceFormatterStyle::MimeList {
        SourceFormatterStyle::MimeHighlightPair { QLatin1String("text/zig"), QLatin1String("Zig") },
        SourceFormatterStyle::MimeHighlightPair { QLatin1String("text/x-zig"), QLatin1String("Zig") }
    });

    QString zigPath = QStandardPaths::findExecutable(QLatin1String("zig"));
    if (zigPath.isEmpty()) {
        qCDebug(KDEV_ZIG) << "Could not find the zig executable";
        zigPath = QLatin1String("/usr/bin/zig");
    }
    zigFormatter.setContent(QLatin1String("%1 fmt $TMPFILE").arg(zigPath));

    return SourceFormatterItemList { SourceFormatterStyleItem { QLatin1String("customscript"), zigFormatter } };
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

}

#include "ziglanguagesupport.moc"
