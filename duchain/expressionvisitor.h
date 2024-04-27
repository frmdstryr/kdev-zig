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
    VisitResult visitErrorValue(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBuiltinCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitFieldAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCmpExpr(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBoolExpr(const ZigNode &node, const ZigNode &parent);
    VisitResult visitMathExpr(const ZigNode &node, const ZigNode &parent);
    VisitResult visitNegation(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBitNot(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBoolNot(const ZigNode &node, const ZigNode &parent);
    VisitResult visitTry(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCatch(const ZigNode &node, const ZigNode &parent);
    VisitResult visitOrelse(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIf(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSwitch(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBlock(const ZigNode &node, const ZigNode &parent);
    VisitResult visitReturn(const ZigNode &node, const ZigNode &parent);
    VisitResult visitFnProto(const ZigNode &node, const ZigNode &parent);

    VisitResult visitArrayType(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayInit(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSlice(const ZigNode &node, const ZigNode &parent);
    VisitResult visitForRange(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayTypeSentinel(const ZigNode &node, const ZigNode &parent);

    VisitResult callBuiltinThis(const ZigNode &node);
    VisitResult callBuiltinTypeInfo(const ZigNode &node);
    VisitResult callBuiltinTypeOf(const ZigNode &node);
    VisitResult callBuiltinImport(const ZigNode &node);
    VisitResult callBuiltinFieldParentPtr(const ZigNode &node);
    VisitResult callBuiltinField(const ZigNode &node);
    VisitResult callBuiltinAs(const ZigNode &node);
    VisitResult callBuiltinVector(const ZigNode &node);
    VisitResult callBuiltinIntFromFloat(const ZigNode &node);
    VisitResult callBuiltinFloatFromInt(const ZigNode &node);
    VisitResult callBuiltinIntCast(const ZigNode &node);
    VisitResult callBuiltinEnumFromInt(const ZigNode &node);
    VisitResult callBuiltinIntFromEnum(const ZigNode &node);
    VisitResult callBuiltinBoolFromInt(const ZigNode &node);
    VisitResult callBuiltinIntFromBool(const ZigNode &node);

    /**
     * Return the self type of a function call node
     * Note the caller must check for nullptr if it the lhs of the call
     * is not a bound function.
     */
    KDevelop::AbstractType::Ptr functionCallSelfType(const ZigNode &owner, const ZigNode &callNode);

    /**
     * Set the current function being visited. This is used by the
     * function visitor to avoid accessing the return type of a recursive
     * function call.
     */
    void setCurrentFunction(const KDevelop::FunctionType::Ptr& fn) {
        m_currentFunction = fn;
    }

    const KDevelop::FunctionType::Ptr& currentFunction() const {
        return m_currentFunction;
    }

    /**
     * Check if the given function is being visited in this or any
     * parent visitor.
     */
    bool isFunctionBeingVisited(const KDevelop::FunctionType::Ptr& fn);


    /**
     * Set the inferred owner type of the visitor. This can be used to help resolve
     * struct/array/enum dot init's
     */
    void setInferredType(const KDevelop::AbstractType::Ptr& t) {
        m_inferredType = t;
    }

    // May be null
    const KDevelop::AbstractType::Ptr& inferredType() const {
        return m_inferredType;
    }

    /**
     * If the expression is a block and a return is was encountered
     */
    void setReturnType(const KDevelop::AbstractType::Ptr& t) {
        m_returnType = t;
    }

    // May be null
    const KDevelop::AbstractType::Ptr& returnType() const {
        return m_returnType;
    }

    void setExcludedDeclaration(const KDevelop::Declaration* d) {
        m_excludedDeclaration = d;
    }

    const KDevelop::Declaration* excludedDeclaration() const {
        return m_excludedDeclaration;
    }

protected:
    ParseSession* m_session;
    KDevelop::AbstractType::Ptr m_inferredType;
    KDevelop::AbstractType::Ptr m_returnType;
    KDevelop::FunctionType::Ptr m_currentFunction;
    const KDevelop::RangeInRevision m_excludedRange = KDevelop::RangeInRevision::invalid();
    const KDevelop::Declaration* m_excludedDeclaration = nullptr;

};

}
