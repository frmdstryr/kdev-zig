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

VisitResult visitCallback(ZAst* ast, uint32_t node, uint32_t parent, void *data);

void ContextBuilder::setParseSession(ParseSession *session)
{
    this->session = session;
}

RangeInRevision ContextBuilder::editorFindSpellingRange(ZigNode &node, const QString &identifier)
{
    SourceRange range = ast_node_spelling_range(node.ast, node.index);
    KTextEditor::Range spellingRange = range.isEmpty() ?
        KTextEditor::Range::invalid() : KTextEditor::Range(
            range.start.line,
            range.start.column,
            range.end.line,
            range.end.column
        );
    return RangeInRevision::castFromSimpleRange(spellingRange);
    // KTextEditor::Range incorrectRange = KTextEditor::Range(range.start.line - 1, range.start.column, INT_MAX, INT_MAX);
    // IDocument *document = ICore::self()->documentController()
    //         ->documentForUrl(topContext()->url().toUrl());
    //
    // QVector<KTextEditor::Range> ranges;
    // if (document) {
    //     ranges = document->textDocument()->searchText(incorrectRange, identifier);
    // } else {
    //     ranges = { KTextEditor::Range::invalid() };
    // }
    //
    //return RangeInRevision::castFromSimpleRange(ranges.first());
}

VisitResult ContextBuilder::visitNode(ZigNode &node, ZigNode &parent)
{
    NodeKind kind = ast_node_kind(node.ast, node.index);

#define BUILD_CONTEXT_FOR(K) case K: return buildContext<K>(node, parent);
    switch (kind) {
    BUILD_CONTEXT_FOR(Module);
    BUILD_CONTEXT_FOR(ContainerDecl);
    BUILD_CONTEXT_FOR(EnumDecl);
    BUILD_CONTEXT_FOR(TemplateDecl);
    BUILD_CONTEXT_FOR(FieldDecl);
    BUILD_CONTEXT_FOR(FunctionDecl);
    BUILD_CONTEXT_FOR(ParmDecl);
    BUILD_CONTEXT_FOR(VarDecl);
    BUILD_CONTEXT_FOR(BlockDecl);
    BUILD_CONTEXT_FOR(ErrorDecl);
    BUILD_CONTEXT_FOR(AliasDecl);
    BUILD_CONTEXT_FOR(TestDecl);

    BUILD_CONTEXT_FOR(Call);
    BUILD_CONTEXT_FOR(ContainerInit);
    BUILD_CONTEXT_FOR(VarAccess);
    BUILD_CONTEXT_FOR(FieldAccess);
    BUILD_CONTEXT_FOR(ArrayAccess);
    BUILD_CONTEXT_FOR(PtrAccess);
    BUILD_CONTEXT_FOR(Literal);
    BUILD_CONTEXT_FOR(Ident);

    BUILD_CONTEXT_FOR(Unknown);

    }
#undef BUILD_CONTEXT_FOR

    return Recurse;
}

template <NodeKind Kind>
VisitResult ContextBuilder::buildContext(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);

    constexpr bool hasContext = NodeTraits::hasContext(Kind);
    QString name = node.spellingName();

    if (hasContext) {
        // qDebug() << "Open context node:" << node.index;
        openContext(&node, NodeTraits::contextType(Kind), &name);
        visitChildren(node);
        closeContext();
        // qDebug() << "Close context node:" << node.index;
        return Continue;
    }
    return Recurse;
}

void ContextBuilder::visitChildren(ZigNode &node)
{
    ast_visit(node.ast, node.index, visitCallback, this);
}

void ContextBuilder::startVisiting(ZigNode *node)
{
    visitChildren(*node);
}

void ContextBuilder::setContextOnNode(ZigNode *node, KDevelop::DUContext *context)
{
    session->setContextOnNode(*node, context);
}

KDevelop::DUContext *ContextBuilder::contextFromNode(ZigNode *node)
{
    return session->contextFromNode(*node);
}

KDevelop::RangeInRevision ContextBuilder::editorFindRange(ZigNode *fromNode, ZigNode *toNode)
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
        file->setLanguage(IndexedString("Zig"));
    }

    return new ZigTopDUContext(document(), range, file);
}

VisitResult visitCallback(ZAst* ast, uint32_t node, uint32_t parent, void *data)
{
    ContextBuilder *builder = static_cast<ContextBuilder *>(data);
    if (builder) {
        ZigNode childNode = {ast, node};
        ZigNode parentNode = {ast, parent};
        return builder->visitNode(childNode, parentNode);
    }
    return VisitResult::Break;

}

QList<Declaration*> ContextBuilder::findSimpleVar(
    QualifiedIdentifier &ident, DUContext* context,
    KDevelop::DUContext::SearchFlag flag
)
{
    QList<Declaration *> declarations;
    if (context) {
        DUContext* parentContext = context;
        while (declarations.isEmpty() && parentContext) {
            declarations = parentContext->findDeclarations(
                ident,
                CursorInRevision::invalid(),
                AbstractType::Ptr(),
                nullptr,
                flag);
            parentContext = parentContext->parentContext();
        }
    }
    return declarations;
}

}
