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

#include "zignode.h"

namespace Zig
{


class KDEVZIGDUCHAIN_EXPORT ExpressionVisitor : public KDevelop::DynamicLanguageExpressionVisitor
{
public:
    explicit ExpressionVisitor(KDevelop::DUContext* context);

    /// Use this to construct the expression-visitor recursively
    ExpressionVisitor(Zig::ExpressionVisitor* parent, const KDevelop::DUContext* overrideContext=nullptr);
    ~ExpressionVisitor() override = default;

    // Visit the nodes children and finally the node
    void startVisiting(ZigNode &node, ZigNode &parent);

    VisitResult visitNode(ZigNode &node, ZigNode &parent);
    void visitChildren(ZigNode &node, ZigNode &parent);

    VisitResult visitContainerDecl(ZigNode &node, ZigNode &parent);
    VisitResult visitPointerType(ZigNode &node, ZigNode &parent);
    VisitResult visitOptionalType(ZigNode &node, ZigNode &parent);
    VisitResult visitStringLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitNumberLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitIdentifier(ZigNode &node, ZigNode &parent);
    VisitResult visitErrorUnion(ZigNode &node, ZigNode &parent);

//private:
//    static QHash<QString, KDevelop::AbstractType::Ptr> m_defaultTypes;
};

}
