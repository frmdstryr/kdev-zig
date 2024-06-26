/*
    SPDX-FileCopyrightText: 2016 Sven Brauch <svenbrauch@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "projectconfigpage.h"

#include "ui_projectconfig.h"

#include "duchain/helpers.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>

namespace Zig {

ProjectConfigPage::ProjectConfigPage(KDevelop::IPlugin* self, const KDevelop::ProjectConfigOptions& options, QWidget* parent)
    : KDevelop::ConfigPage(self, nullptr, parent)
    , m_ui(new Ui_ProjectConfig)
{
    m_configGroup = options.project->projectConfiguration()->group(QLatin1String("kdevzigsupport"));
    m_ui->setupUi(this);
    m_project = options.project;
    // So apply button activates
    connect(m_ui->zigExecutable, &QLineEdit::textChanged, this, &ProjectConfigPage::changed);
    connect(m_ui->zigPackages, &QTextEdit::textChanged, this, &ProjectConfigPage::changed);
    connect(m_ui->zigTargetPtrSize, &QSpinBox::valueChanged, this, &ProjectConfigPage::changed);

}

void Zig::ProjectConfigPage::apply()
{
    m_configGroup.writeEntry("zigExecutable", m_ui->zigExecutable->text());
    QString pkgs = m_ui->zigPackages->toPlainText();
    if (!pkgs.endsWith(QLatin1Char('\n'))) {
        pkgs.append(QLatin1Char('\n'));
    }
    m_configGroup.writeEntry("zigPackages",  pkgs);
    m_configGroup.writeEntry("zigTargetPtrSize",  m_ui->zigTargetPtrSize->value());

    // remove cached paths, so they are updated on the next parse job run
    {
        QMutexLocker lock(&Helper::cacheMutex);
        Helper::cachedSearchPaths.remove(m_project);
    }
    {
        QMutexLocker lock(&Helper::projectPathLock);
        Helper::projectPackagesLoaded.clear();
        Helper::projectPackages.clear();
        Helper::projectTargetPointerSizes.clear();
    }
}

void Zig::ProjectConfigPage::defaults()
{
    m_ui->zigExecutable->setText(QString());
}

void Zig::ProjectConfigPage::reset()
{
    m_ui->zigExecutable->setText(m_configGroup.readEntry("zigExecutable"));
    m_ui->zigPackages->setText(m_configGroup.readEntry("zigPackages"));
    m_ui->zigTargetPtrSize->setValue(m_configGroup.readEntry("zigTargetPtrSize", 0));
}

QString Zig::ProjectConfigPage::name() const
{
    return i18n("Zig Settings");
}

}


