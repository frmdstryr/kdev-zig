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
#include "zigdebug.h"
#include "parsesession.h"

namespace Zig
{


class KDEVZIGDUCHAIN_EXPORT ExpressionVisitor : public KDevelop::DynamicLanguageExpressionVisitor
{
public:
    explicit ExpressionVisitor(ParseSession* session, const KDevelop::DUContext* context);

    /// Use this to construct the expression-visitor recursively
    ExpressionVisitor(Zig::ExpressionVisitor* parent, const KDevelop::DUContext* overrideContext=nullptr);
    ~ExpressionVisitor() override = default;

    ParseSession* session() {return m_session;}

    // Visit the nodes children and finally the node
    void startVisiting(const ZigNode &node, const ZigNode &parent);

    VisitResult visitNode(const ZigNode &node, const ZigNode &parent);
    void visitChildren(const ZigNode &node, const ZigNode &parent);

    VisitResult visitStructInit(const ZigNode &node, const ZigNode &parent);
    VisitResult visitContainerDecl(const ZigNode &node, const ZigNode &parent);
    VisitResult visitAddressOf(const ZigNode &node, const ZigNode &parent);
    VisitResult visitDeref(const ZigNode &node, const ZigNode &parent);
    VisitResult visitPointerType(const ZigNode &node, const ZigNode &parent);
    VisitResult visitOptionalType(const ZigNode &node, const ZigNode &parent);
    VisitResult visitUnwrapOptional(const ZigNode &node, const ZigNode &parent);
    VisitResult visitStringLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCharLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitMultilineStringLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitNumberLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitEnumLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIdentifier(const ZigNode &node, const ZigNode &parent);
    VisitResult visitErrorUnion(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBuiltinCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitFieldAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBoolExpr(const ZigNode &node, const ZigNode &parent);
    VisitResult visitMathExpr(const ZigNode &node, const ZigNode &parent);
    VisitResult visitNegation(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBitNot(const ZigNode &node, const ZigNode &parent);
    VisitResult visitTry(const ZigNode &node, const ZigNode &parent);
    VisitResult visitOrelse(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIf(const ZigNode &node, const ZigNode &parent);

    VisitResult visitPointerTypeAligned(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayType(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayInit(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSlice(const ZigNode &node, const ZigNode &parent);
    VisitResult visitForRange(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayTypeSentinel(const ZigNode &node, const ZigNode &parent);

    VisitResult callBuiltinThis(const ZigNode &node);
    VisitResult callBuiltinImport(const ZigNode &node);
    VisitResult callBuiltinFieldParentPtr(const ZigNode &node);

    // Imported contexts
    void encounterTopContext(const KDevelop::TopDUContext* ctx)
    {
        // qCDebug(KDEV_ZIG) << "Encounter top context" << ctx;
        m_lastTopContext = ctx;
    }

    const KDevelop::TopDUContext* lastTopContext() const
    {
        return m_lastTopContext ? m_lastTopContext: topContext();
    }

protected:
    ParseSession* m_session;
    const KDevelop::TopDUContext* m_lastTopContext = nullptr;
    const KDevelop::RangeInRevision m_excludedRange = KDevelop::RangeInRevision::invalid();

};

}
