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

#include "zigducontext.h"
#include "nodetraits.h"

namespace Zig
{

ZVisitResult visitCallback(ZNode *node, ZNode *parent, void *data);

void ContextBuilder::setParseSession(ParseSession *session)
{
    this->session = session;
}

RangeInRevision ContextBuilder::editorFindSpellingRange(ZigNode *node, const QString &identifier)
{
    ZAstRange range = ast_node_spelling_range(node->data());
    KTextEditor::Range incorrectRange = KTextEditor::Range(range.start.line - 1, range.start.column, INT_MAX, INT_MAX);
    IDocument *document = ICore::self()->documentController()
            ->documentForUrl(topContext()->url().toUrl());

    QVector<KTextEditor::Range> ranges;
    if (document) {
        ranges = document->textDocument()->searchText(incorrectRange, identifier);
    } else {
        ranges = { KTextEditor::Range::invalid() };
    }

    return RangeInRevision::castFromSimpleRange(ranges.first());
}

ZVisitResult ContextBuilder::visitNode(ZigNode *node, ZigNode *parent)
{
    ZNodeKind kind = ast_node_kind(node->data());

#define BUILD_CONTEXT_FOR(K) case K: return buildContext<K>(node, parent);
    switch (kind) {
    //BUILD_CONTEXT_FOR(Crate);
    BUILD_CONTEXT_FOR(Module);
    BUILD_CONTEXT_FOR(StructDecl);
    BUILD_CONTEXT_FOR(EnumDecl);
    BUILD_CONTEXT_FOR(TraitDecl);
    BUILD_CONTEXT_FOR(ImplDecl);
    BUILD_CONTEXT_FOR(TypeAliasDecl);
    BUILD_CONTEXT_FOR(FieldDecl);
    BUILD_CONTEXT_FOR(EnumVariantDecl);
    BUILD_CONTEXT_FOR(FunctionDecl);
    BUILD_CONTEXT_FOR(ParmDecl);
    BUILD_CONTEXT_FOR(VarDecl);
    BUILD_CONTEXT_FOR(Path);
    BUILD_CONTEXT_FOR(PathSegment);
    BUILD_CONTEXT_FOR(Block);
    BUILD_CONTEXT_FOR(Arm);
    BUILD_CONTEXT_FOR(Unexposed);
    }
#undef BUILD_CONTEXT_FOR

    return Recurse;
}

template <ZNodeKind Kind>
ZVisitResult ContextBuilder::buildContext(ZigNode *node, ZigNode *parent)
{
    Q_UNUSED(parent);

    constexpr bool hasContext = NodeTraits::hasContext(Kind);
    ZigPath name(node);

    if (hasContext) {
        openContext(node, NodeTraits::contextType(Kind), &name);
        visitChildren(node);
        closeContext();
        return Continue;
    }
    return Recurse;
}

void ContextBuilder::visitChildren(ZigNode *node)
{
    ast_visit_children(node->data(), visitCallback, this);
}

void ContextBuilder::startVisiting(ZigNode *node)
{
    visitChildren(node);
}

void ContextBuilder::setContextOnNode(ZigNode *node, KDevelop::DUContext *context)
{
    session->setContextOnNode(node, context);
}

KDevelop::DUContext *ContextBuilder::contextFromNode(ZigNode *node)
{
    return session->contextFromNode(node);
}

KDevelop::RangeInRevision ContextBuilder::editorFindRange(ZigNode *fromNode, ZigNode *toNode)
{
    ZAstRange fromRange = ast_node_extent(fromNode->data());
    ZAstRange toRange = ast_node_extent(toNode->data());

    return RangeInRevision(fromRange.start.line - 1, fromRange.start.column, toRange.end.line - 1, toRange.end.column);
}

KDevelop::QualifiedIdentifier ContextBuilder::identifierForNode(ZigPath *node)
{
    return QualifiedIdentifier(node->value);
}

KDevelop::DUContext *ContextBuilder::newContext(const KDevelop::RangeInRevision &range)
{
    return new ZigNormalDUContext(range, currentContext());
}

KDevelop::TopDUContext *ContextBuilder::newTopContext(const KDevelop::RangeInRevision &range, KDevelop::ParsingEnvironmentFile *file)
{
    if (!file) {
        file = new ParsingEnvironmentFile(document());
        file->setLanguage(IndexedString("Zig"));
    }

    return new ZigTopDUContext(document(), range, file);
}

ZVisitResult visitCallback(ZNode *node, ZNode *parent, void *data)
{
    ContextBuilder *builder = static_cast<ContextBuilder *>(data);
    ZigNode currentNode(node);
    ZigNode parentNode(parent);
    return builder->visitNode(&currentNode, &parentNode);
}

}
