/*
    SPDX-FileCopyrightText: 2016 Sven Brauch <svenbrauch@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "projectconfigpage.h"

#include "ui_projectconfig.h"

#include "duchain/helpers.h"
#include <QLineEdit>

namespace Zig {

ProjectConfigPage::ProjectConfigPage(KDevelop::IPlugin* self, const KDevelop::ProjectConfigOptions& options, QWidget* parent)
    : KDevelop::ConfigPage(self, nullptr, parent)
    , m_ui(new Ui_ProjectConfig)
{
    m_configGroup = options.project->projectConfiguration()->group("kdevzigsupport");
    m_ui->setupUi(this);
    m_project = options.project;
    // So apply button activates
    connect(m_ui->zigExecutable, &QLineEdit::textChanged, this, &ProjectConfigPage::changed);
}

void Zig::ProjectConfigPage::apply()
{
    m_configGroup.writeEntry("zigExecutable", m_ui->zigExecutable->text());
    // remove cached paths, so they are updated on the next parse job run
    QMutexLocker lock(&Helper::cacheMutex);
    Helper::cachedSearchPaths.remove(m_project);
}

void Zig::ProjectConfigPage::defaults()
{
    m_ui->zigExecutable->setText(QString());
}

void Zig::ProjectConfigPage::reset()
{
    m_ui->zigExecutable->setText(m_configGroup.readEntry("zigExecutable"));
}

QString Zig::ProjectConfigPage::name() const
{
    return i18n("Zig Settings");
}

}


