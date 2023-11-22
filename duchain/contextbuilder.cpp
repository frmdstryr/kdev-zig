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

#include "contextbuilder.h"

#include <language/duchain/parsingenvironment.h>
#include <interfaces/icore.h>
#include <interfaces/idocumentcontroller.h>
#include <language/duchain/declaration.h>

#include "zigducontext.h"
#include "helpers.h"
#include "nodetraits.h"

namespace Zig
{

VisitResult contextBuilderVisitorCallback(ZAst* ast, NodeIndex node, NodeIndex parent, void *data)
{
    ContextBuilder *visitor = static_cast<ContextBuilder *>(data);
    Q_ASSERT(visitor);
    ZigNode childNode = {ast, node};
    ZigNode parentNode = {ast, parent};
    return visitor->visitNode(childNode, parentNode);
}

void ContextBuilder::setParseSession(ParseSession *session)
{
    this->session = session;
}

RangeInRevision ContextBuilder::editorFindSpellingRange(const ZigNode &node, const QString &identifier)
{
    Q_UNUSED(identifier);
    TokenIndex tok = ast_node_name_token(node.ast, node.index);
    SourceRange range = ast_token_range(node.ast, tok);
    KTextEditor::Range spellingRange = range.isEmpty() ?
        KTextEditor::Range::invalid() : KTextEditor::Range(
            range.start.line,
            range.start.column,
            range.end.line,
            range.end.column
        );
    return RangeInRevision::castFromSimpleRange(spellingRange);
}

VisitResult ContextBuilder::visitNode(const ZigNode &node, const ZigNode &parent)
{
    NodeKind kind = node.kind();
    // qDebug() << "ContextBuilder::visitNode" << node.index;

#define BUILD_CONTEXT_FOR(K) case K: return buildContext<K>(node, parent);
    switch (kind) {
    BUILD_CONTEXT_FOR(Module);
    BUILD_CONTEXT_FOR(ContainerDecl);
    BUILD_CONTEXT_FOR(EnumDecl);
    BUILD_CONTEXT_FOR(UnionDecl);
    BUILD_CONTEXT_FOR(FieldDecl);
    BUILD_CONTEXT_FOR(FunctionDecl);
    BUILD_CONTEXT_FOR(ParamDecl);
    BUILD_CONTEXT_FOR(VarDecl);
    BUILD_CONTEXT_FOR(BlockDecl);
    BUILD_CONTEXT_FOR(ErrorDecl);
    BUILD_CONTEXT_FOR(TestDecl);

    BUILD_CONTEXT_FOR(Call);
    BUILD_CONTEXT_FOR(If);
    BUILD_CONTEXT_FOR(For);
    BUILD_CONTEXT_FOR(While);
    BUILD_CONTEXT_FOR(Switch);
    BUILD_CONTEXT_FOR(Defer);
    BUILD_CONTEXT_FOR(Catch);
    BUILD_CONTEXT_FOR(Usingnamespace);

    BUILD_CONTEXT_FOR(Unknown);

    }
#undef BUILD_CONTEXT_FOR

    return Recurse;
}

bool ContextBuilder::shouldSkipNode(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    if (node.kind() == VarDecl) {
        // When we get a var decl followed by a container or error
        // decl, skip making a separate declaration/context for the
        // variable and just use the name.
        // Eg const Foo = struct {}
        ZigNode child = node.rhsAsNode();
        NodeKind childKind = child.kind();
        return (
            childKind == ContainerDecl
            || childKind == ErrorDecl
            || childKind == EnumDecl
            || childKind == UnionDecl
            //|| (childKind == Call && child.spellingName() == "@import")
        );
    }
    return false;
}

void ContextBuilder::visitChildren(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ast_visit(node.ast, node.index, contextBuilderVisitorCallback, this);
}

template <NodeKind Kind>
VisitResult ContextBuilder::buildContext(const ZigNode &node, const ZigNode &parent)
{
    if (shouldSkipNode(node, parent)) {
        return Recurse; // Skip this case
    }
    if (NodeTraits::hasChildren(Kind)) {
        constexpr bool hasContext = NodeTraits::hasContext(Kind);
        if (hasContext) {
            bool overwrite = NodeTraits::shouldUseParentName(Kind, parent.kind());
            QString name = overwrite ? parent.spellingName() : node.spellingName();
            DUChainWriteLocker lock;
            openContext(&node, NodeTraits::contextType(Kind), &name);
        }
        visitChildren(node, parent);
        if (hasContext) {
            closeContext();
        }
        return Continue;
    }
    return Recurse;
}

void ContextBuilder::startVisiting(const ZigNode *node)
{
    visitNode(*node, *node);
}

void ContextBuilder::setContextOnNode(const ZigNode *node, KDevelop::DUContext *context)
{
    session->setContextOnNode(*node, context);
}

KDevelop::DUContext *ContextBuilder::contextFromNode(const ZigNode *node)
{
    return session->contextFromNode(*node);
}

KDevelop::RangeInRevision ContextBuilder::editorFindRange(const ZigNode *fromNode, const ZigNode *toNode)
{
    SourceRange fromRange = fromNode->extent();
    SourceRange toRange = (fromNode == toNode) ? fromRange : toNode->extent();

    return RangeInRevision(
        fromRange.start.line, fromRange.start.column,
        toRange.end.line, toRange.end.column
    );
}

KDevelop::QualifiedIdentifier ContextBuilder::identifierForNode(QString *node)
{
    if (node && !node->isEmpty()) {
        QString qualifier = Helper::qualifierPath(session->document().str());
        QString ident = qualifier.isEmpty() ? *node : qualifier + '.' + *node;
        // qDebug() << "ident" << ident;
        return QualifiedIdentifier(ident);
    }
    return QualifiedIdentifier(*node);
}

KDevelop::DUContext *ContextBuilder::newContext(const KDevelop::RangeInRevision &range)
{
    return new ZigNormalDUContext(range, currentContext());
}

KDevelop::TopDUContext *ContextBuilder::newTopContext(const KDevelop::RangeInRevision &range, KDevelop::ParsingEnvironmentFile *file)
{
    if (!file) {
        file = new ParsingEnvironmentFile(document());
        file->setLanguage(ParseSession::languageString());
    }

    return new ZigTopDUContext(document(), range, file);
}

}
