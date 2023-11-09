/*
    SPDX-FileCopyrightText: 2023 Jairus Martin <frmdstryr@protonmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <language/duchain/ducontext.h>
#include "zignode.h"
#include "parsesession.h"


namespace Zig
{

/**
 * Visitor that attempts to determine the return type of a generic function
 *
 */
class KDEVZIGDUCHAIN_EXPORT FunctionVisitor
{
public:
    explicit FunctionVisitor(ParseSession* session, KDevelop::DUContext* context);

    /// Use this to construct the expression-visitor recursively
    explicit FunctionVisitor(FunctionVisitor* parent, const KDevelop::DUContext* overrideContext=nullptr);
    ~FunctionVisitor() = default;

    ParseSession* session() {return m_session;}

    /**
     * Start visiting the function body.
     * The parent must be the function declaration node
     */
    void startVisiting(const ZigNode &node, const ZigNode &parent);

    VisitResult visitNode(const ZigNode &node, const ZigNode &parent);
    void visitChildren(const ZigNode &node, const ZigNode &parent);

    VisitResult visitReturn(const ZigNode &node, const ZigNode &parent);

    /**
     * @brief Return the DUContext this visitor is working on.
     */
    inline const KDevelop::DUContext* context() const
    {
        return m_context;
    }

    inline const KDevelop::TopDUContext* topContext() const
    {
        return m_context->topContext();
    }

    /**
     * Encounter a return statement
     */
    void encounterReturn(const KDevelop::AbstractType::Ptr& type,
                         const KDevelop::DeclarationPointer& declaration = KDevelop::DeclarationPointer())
    {
        m_returnCount += 1;
        m_returnType = type;
        m_returnDeclaration = declaration;
    }

    /**
     * @brief Retrieve this visitor's last encountered type.
     * This is never a null type.
     */
    inline KDevelop::AbstractType::Ptr returnType() const
    {
        if (!m_returnType || m_returnCount != 1) {
            return unknownType();
        }
        return m_returnType;
    }

    inline uint32_t returnCount() const
    {
        return m_returnCount;
    }

    /**
     * @brief Retrieve this visitor's last encountered return declaration. May be null.
     */
    inline KDevelop::DeclarationPointer returnDeclaration() const
    {
        return m_returnDeclaration;
    }

    KDevelop::AbstractType::Ptr unknownType() const;

protected:
    const KDevelop::DUContext* m_context;
    ParseSession* m_session;
    FunctionVisitor* m_parentVisitor = nullptr;
    uint32_t m_returnCount = 0;
    KDevelop::AbstractType::Ptr m_returnType;
    KDevelop::DeclarationPointer m_returnDeclaration;

};

}
