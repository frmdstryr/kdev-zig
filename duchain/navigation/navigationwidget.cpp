/*
    SPDX-FileCopyrightText: 2014 Milian Wolff <mail@milianw.de>
    SPDX-FileCopyrightText: 2014 Kevin Funk <kfunk@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "navigationwidget.h"

#include <language/duchain/navigation/abstractdeclarationnavigationcontext.h>
#include <language/duchain/navigation/abstractincludenavigationcontext.h>
#include <language/util/includeitem.h>

using namespace KDevelop;

namespace Zig {

class DeclarationNavigationContext : public AbstractDeclarationNavigationContext
{
    Q_OBJECT
public:
    using AbstractDeclarationNavigationContext::AbstractDeclarationNavigationContext;

    void htmlIdentifiedType(AbstractType::Ptr type, const IdentifiedType* idType) override
    {
        AbstractDeclarationNavigationContext::htmlIdentifiedType(type, idType);

        // if (auto cst = dynamic_cast<const ClassSpecializationType*>(type.data())) {
        //     modifyHtml() += QStringLiteral("< ").toHtmlEscaped();
        //
        //     bool first = true;
        //     const auto& templateParameters = cst->templateParameters();
        //     for (const auto& type : templateParameters) {
        //         if (first) {
        //             first = false;
        //         } else {
        //             modifyHtml() += QStringLiteral(", ");
        //         }
        //
        //         eventuallyMakeTypeLinks(type.abstractType());
        //     }
        //
        //     modifyHtml() += QStringLiteral(" >").toHtmlEscaped();
        // }
    }
};

class IncludeNavigationContext : public KDevelop::AbstractIncludeNavigationContext
{
    Q_OBJECT
public:
    IncludeNavigationContext(const KDevelop::IncludeItem& item, const KDevelop::TopDUContextPointer& topContext);

protected:
    bool filterDeclaration(KDevelop::Declaration* decl) override;
};

IncludeNavigationContext::IncludeNavigationContext(const IncludeItem& item, const KDevelop::TopDUContextPointer& topContext)
    : AbstractIncludeNavigationContext(item, topContext, StandardParsingEnvironment)
{}

bool IncludeNavigationContext::filterDeclaration(Declaration* decl)
{
    QString declId = decl->identifier().identifier().str();
    //filter out forward-declarations and macro-expansions without a range
    //And filter out declarations with reserved identifiers
    return !decl->qualifiedIdentifier().toString().isEmpty() && !decl->range().isEmpty() && !decl->isForwardDeclaration()
            && !(declId.startsWith(QLatin1String("__")) || (declId.startsWith(QLatin1Char('_')) && declId.length() > 1 && declId[1].isUpper()) );
}

NavigationWidget::NavigationWidget(const DeclarationPointer& declaration, KDevelop::AbstractNavigationWidget::DisplayHints hints)
    : AbstractNavigationWidget()
{
    setDisplayHints(hints);
    initBrowser(400);
    setContext(NavigationContextPointer(new DeclarationNavigationContext(declaration, {})));
}

NavigationWidget::NavigationWidget(const IncludeItem& includeItem, const KDevelop::TopDUContextPointer& topContext,
                                             KDevelop::AbstractNavigationWidget::DisplayHints hints)
    : AbstractNavigationWidget()
{
    setDisplayHints(hints);
    initBrowser(200);

    //The first context is registered so it is kept alive by the shared-pointer mechanism
    auto context = new IncludeNavigationContext(includeItem, topContext);
    setContext(NavigationContextPointer(context));
}

QString NavigationWidget::shortDescription(const IncludeItem& includeItem)
{
  IncludeNavigationContext ctx(includeItem, {});
  return ctx.html(true);
}

#include "navigationwidget.moc"
#include "moc_navigationwidget.cpp"

}
