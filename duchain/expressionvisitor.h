/*
    SPDX-FileCopyrightText: 2013 Andrea Scarpino <scarpino@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QString>
#include <language/duchain/builders/dynamiclanguageexpressionvisitor.h>
#include <language/duchain/types/integraltype.h>
#include <language/duchain/ducontext.h>

#include "visitor.h"

namespace Zig
{


class KDEVZIGDUCHAIN_EXPORT ExpressionVisitor : public KDevelop::DynamicLanguageExpressionVisitor, public Visitor
{
public:
    explicit ExpressionVisitor(KDevelop::DUContext* context);

    /// Use this to construct the expression-visitor recursively
    ExpressionVisitor(Zig::ExpressionVisitor* parent, const KDevelop::DUContext* overrideContext=nullptr);
    ~ExpressionVisitor() override = default;

    virtual VisitResult visitNode(ZigNode &node, ZigNode &parent) override;
    VisitResult visitPointerType(ZigNode &node);
    VisitResult visitOptionalType(ZigNode &node);
    VisitResult visitStringLiteral(ZigNode &node);
    VisitResult visitNumberLiteral(ZigNode &node);
    VisitResult visitIdentifier(ZigNode &node);

//private:
//    static QHash<QString, KDevelop::AbstractType::Ptr> m_defaultTypes;
};

}
