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

#include <language/duchain/builders/abstractcontextbuilder.h>

#include "parsesession.h"
#include "zignode.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

using ContextBuilderBase = KDevelop::AbstractContextBuilder<ZigNode, ZigPath>;

class KDEVZIGDUCHAIN_EXPORT ContextBuilder : public ContextBuilderBase
{
public:
    ContextBuilder() = default;
    ~ContextBuilder() override = default;

    void setParseSession(ParseSession *session);

protected:
    KDevelop::RangeInRevision editorFindSpellingRange(ZigNode *node, const QString &identifier);

    template <ZNodeKind>
    ZVisitResult buildContext(ZigNode *node, ZigNode *parent);

    template<ZNodeKind Kind>
    KDevelop::DUContext *createContext(ZNode *node, const KDevelop::QualifiedIdentifier& scopeId);

    virtual ZVisitResult visitNode(ZigNode *node, ZigNode *parent);
    void visitChildren(ZigNode *node);

    void startVisiting(ZigNode *node) override;
    void setContextOnNode(ZigNode *node, KDevelop::DUContext *context) override;
    KDevelop::DUContext *contextFromNode(ZigNode *node) override;
    KDevelop::RangeInRevision editorFindRange(ZigNode *fromNode, ZigNode *toNode) override;
    KDevelop::QualifiedIdentifier identifierForNode(ZigPath *node) override;
    KDevelop::DUContext *newContext(const KDevelop::RangeInRevision &range) override;
    KDevelop::TopDUContext *newTopContext(const KDevelop::RangeInRevision &range, KDevelop::ParsingEnvironmentFile *file) override;

private:
    friend ZVisitResult visitCallback(ZNode *node, ZNode *parent, void *data);

    ParseSession *session;
};

}

#endif // CONTEXTBUILDER_H
