/*
    SPDX-FileCopyrightText: 2014 Milian Wolff <mail@milianw.de>
    SPDX-FileCopyrightText: 2014 Kevin Funk <kfunk@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <language/duchain/navigation/abstractnavigationwidget.h>
#include "kdevzigduchain_export.h"

namespace KDevelop
{
class DocumentCursor;
class IncludeItem;
}

namespace Zig {

class KDEVZIGDUCHAIN_EXPORT NavigationWidget : public KDevelop::AbstractNavigationWidget
{
    Q_OBJECT
public:
    explicit NavigationWidget(const KDevelop::DeclarationPointer& declaration,
                          KDevelop::AbstractNavigationWidget::DisplayHints hints = KDevelop::AbstractNavigationWidget::NoHints);
    NavigationWidget(const KDevelop::IncludeItem& includeItem, const KDevelop::TopDUContextPointer& topContext,
                          KDevelop::AbstractNavigationWidget::DisplayHints hints = KDevelop::AbstractNavigationWidget::NoHints);
    ~NavigationWidget() override = default;

    /// Used by @see AbstractIncludeFileCompletionItem
    static QString shortDescription(const KDevelop::IncludeItem& includeItem);
};

}
