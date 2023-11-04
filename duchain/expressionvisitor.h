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
#include "parsesession.h"

namespace Zig
{


class KDEVZIGDUCHAIN_EXPORT ExpressionVisitor : public KDevelop::DynamicLanguageExpressionVisitor
{
public:
    explicit ExpressionVisitor(ParseSession* session, KDevelop::DUContext* context);

    /// Use this to construct the expression-visitor recursively
    ExpressionVisitor(Zig::ExpressionVisitor* parent, const KDevelop::DUContext* overrideContext=nullptr);
    ~ExpressionVisitor() override = default;

    ParseSession* session() {return m_session;}

    // Visit the nodes children and finally the node
    void startVisiting(ZigNode &node, ZigNode &parent);

    VisitResult visitNode(ZigNode &node, ZigNode &parent);
    void visitChildren(ZigNode &node, ZigNode &parent);

    VisitResult visitStructInit(ZigNode &node, ZigNode &parent);
    VisitResult visitContainerDecl(ZigNode &node, ZigNode &parent);
    VisitResult visitAddressOf(ZigNode &node, ZigNode &parent);
    VisitResult visitDeref(ZigNode &node, ZigNode &parent);
    VisitResult visitPointerType(ZigNode &node, ZigNode &parent);
    VisitResult visitOptionalType(ZigNode &node, ZigNode &parent);
    VisitResult visitUnwrapOptional(ZigNode &node, ZigNode &parent);
    VisitResult visitStringLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitCharLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitMultilineStringLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitNumberLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitEnumLiteral(ZigNode &node, ZigNode &parent);
    VisitResult visitIdentifier(ZigNode &node, ZigNode &parent);
    VisitResult visitErrorUnion(ZigNode &node, ZigNode &parent);
    VisitResult visitCall(ZigNode &node, ZigNode &parent);
    VisitResult visitBuiltinCall(ZigNode &node, ZigNode &parent);
    VisitResult visitFieldAccess(ZigNode &node, ZigNode &parent);
    VisitResult visitBoolExpr(ZigNode &node, ZigNode &parent);
    VisitResult visitMathExpr(ZigNode &node, ZigNode &parent);
    VisitResult visitNegation(ZigNode &node, ZigNode &parent);
    VisitResult visitBitNot(ZigNode &node, ZigNode &parent);
    VisitResult visitTry(ZigNode &node, ZigNode &parent);
    VisitResult visitOrelse(ZigNode &node, ZigNode &parent);
    VisitResult visitIf(ZigNode &node, ZigNode &parent);



    VisitResult visitPointerTypeAligned(ZigNode &node, ZigNode &parent);
    VisitResult visitArrayType(ZigNode &node, ZigNode &parent);
    VisitResult visitSlice(ZigNode &node, ZigNode &parent);
    VisitResult visitArrayAccess(ZigNode &node, ZigNode &parent);
    VisitResult visitArrayTypeSentinel(ZigNode &node, ZigNode &parent);

    VisitResult callBuiltinThis(ZigNode &node);
    VisitResult callBuiltinImport(ZigNode &node);

protected:
    ParseSession* m_session;

};

}
