/*
 * Copyright 2017  Emma Gospodinova <emma.gospodinova@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONTEXTBUILDER_H
#define CONTEXTBUILDER_H

#include <QString>
#include <language/duchain/builders/abstractcontextbuilder.h>

#include "parsesession.h"
#include "zignode.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

using ContextBuilderBase = KDevelop::AbstractContextBuilder<ZigNode, QString>;

class KDEVZIGDUCHAIN_EXPORT ContextBuilder : public ContextBuilderBase
{
public:
    ContextBuilder() = default;
    ~ContextBuilder() override = default;

    void setParseSession(ParseSession *session);
    virtual VisitResult visitNode(ZigNode &node, ZigNode &parent);
protected:
    KDevelop::RangeInRevision editorFindSpellingRange(ZigNode &node, const QString &identifier);

    template <NodeKind>
    VisitResult buildContext(ZigNode &node, ZigNode &parent);

    template<NodeKind Kind>
    KDevelop::DUContext *createContext(ZigNode *node, const KDevelop::QualifiedIdentifier& scopeId);

    void visitChildren(ZigNode &node);

    void startVisiting(ZigNode *node) override;
    void setContextOnNode(ZigNode *node, KDevelop::DUContext *context) override;
    KDevelop::DUContext *contextFromNode(ZigNode *node) override;
    KDevelop::RangeInRevision editorFindRange(ZigNode *fromNode, ZigNode *toNode) override;
    KDevelop::QualifiedIdentifier identifierForNode(QString *node) override;
    KDevelop::DUContext *newContext(const KDevelop::RangeInRevision &range) override;
    KDevelop::TopDUContext *newTopContext(const KDevelop::RangeInRevision &range, KDevelop::ParsingEnvironmentFile *file) override;

    // Find any declarations matching ident in this or any parent scope
    // Caller must be holding the read lock
    // TODO: Does something like this already exist?
    QList<KDevelop::Declaration*> findSimpleVar(
        KDevelop::QualifiedIdentifier &ident,
        KDevelop::DUContext* context,
        KDevelop::DUContext::SearchFlag flag = KDevelop::DUContext::SearchFlag::NoSearchFlags
    );


private:
    friend VisitResult visitCallback(ZigNode *node, ZigNode *parent, void *data);

    ParseSession *session;
};

}

#endif // CONTEXTBUILDER_H
