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

#include "usebuilder.h"

#include <language/duchain/ducontext.h>
#include <language/duchain/declaration.h>
#include <language/duchain/problem.h>
#include <language/editor/documentrange.h>

#include "zigdebug.h"

namespace Zig
{

using namespace KDevelop;

UseBuilder::UseBuilder(const KDevelop::IndexedString &document)
    : document(document)
{
}

ZVisitResult UseBuilder::visitNode(ZNode &node, ZNode &parent)
{
    ZNodeKind kind = ast_node_kind(node);
    ZNodeKind parentKind = ast_node_kind(parent);

    switch (kind){
        case Call:
            visitCall(node, parent);
            break;
        case ContainerInit:
            visitContainerInit(node, parent);
            break;
        case VarAccess:
            visitVarAccess(node, parent);
            break;
        case FieldAccess:
            visitFieldAccess(node, parent);
            break;
        case ArrayAccess:
            visitArrayAccess(node, parent);
            break;
        case PtrAccess:
            visitPtrAccess(node, parent);
            break;
        case Literal:
            visitLiteral(node, parent);
            break;
        case Ident:
            visitIdent(node, parent);
            break;
        default:
            break;
    }
    return ContextBuilder::visitNode(node, parent);
}

void UseBuilder::visitCall(ZNode &node, ZNode &parent)
{
    // TODO
}

void UseBuilder::visitContainerInit(ZNode &node, ZNode &parent)
{
    ZigPath containerName(node);
    RangeInRevision useRange = editorFindSpellingRange(node, containerName.value);

    IndexedIdentifier identifier = IndexedIdentifier(Identifier(containerName.value));
    currentPath.clear();
    currentPath.push(identifier);
    if (containerName.value.isEmpty()) {
        return;
    }
    if (containerName.value == ".") {
        return;  // TODO: Handle .
    }

    DUChainWriteLocker lock(DUChain::lock());
    DUContext *context = topContext()->findContextAt(useRange.start);
    if (!context) return;

    QList<Declaration *> declarations = context->findDeclarations(
        currentPath,
        CursorInRevision::invalid(),
        AbstractType::Ptr(),
        nullptr,
        DUContext::NoSearchFlags);

    // TODO: Find only structs
    DUContext* parentContext = context->parentContext();
    while (declarations.isEmpty() && parentContext) {
        declarations = parentContext->findDeclarations(
            currentPath,
            CursorInRevision::invalid(),
            AbstractType::Ptr(),
            nullptr,
            DUContext::NoSearchFlags);
    }

    if (declarations.isEmpty()) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Undefined %1", containerName.value));
        topContext()->addProblem(p);
    } else {
        for (Declaration *declaration : declarations) {
            // TODO: Check if a struct or alias of struct
            if (declaration->range() != useRange) {
                UseBuilderBase::newUse(useRange, DeclarationPointer(declaration));
                break;
            }
        }
    }

}

void UseBuilder::visitVarAccess(ZNode &node, ZNode &parent)
{
    // TODO
}

void UseBuilder::visitFieldAccess(ZNode &node, ZNode &parent)
{
    // TODO
}

void UseBuilder::visitArrayAccess(ZNode &node, ZNode &parent)
{
    // TODO
}

void UseBuilder::visitPtrAccess(ZNode &node, ZNode &parent)
{
    // TODO
}

void UseBuilder::visitIdent(ZNode &node, ZNode &parent)
{
    // TODO
}

void UseBuilder::visitLiteral(ZNode &node, ZNode &parent)
{
    // TODO
}

// void UseBuilder::visitPath(ZNode &node, ZNode &parent)
// {
//     ZigPath path(node);
//     fullPath = identifierForNode(&path);
//     currentPath.clear();
// }
//
// void UseBuilder::visitPathSegment(ZNode &node, ZNode &parent)
// {
//     ZigPath segment(node);
//     IndexedIdentifier pathSegment = IndexedIdentifier(Identifier(segment.value));
//
//     currentPath.push(pathSegment);
//
//     DUContext::SearchFlags flags = DUContext::NoSearchFlags;
//
//     if (fullPath.isQualified()) {
//         flags = DUContext::NoFiltering;
//     }
//
//     RangeInRevision useRange = editorFindRange(&node, &node);
//     DUContext *context = topContext()->findContextAt(useRange.start);
//     QList<Declaration *> declarations = context->findDeclarations(currentPath,
//                                                                   CursorInRevision::invalid(),
//                                                                   AbstractType::Ptr(),
//                                                                   nullptr,
//                                                                   flags);
//
//     if (declarations.isEmpty() || !declarations.first()) {
//         ProblemPointer p = ProblemPointer(new Problem());
//         p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
//         p->setSource(IProblem::SemanticAnalysis);
//         p->setSeverity(IProblem::Hint);
//         p->setDescription(i18n("Undefined %1", fullPath.toString()));
//
//         DUChainWriteLocker lock(DUChain::lock());
//         topContext()->addProblem(p);
//     } else {
//         for (Declaration *declaration : declarations) {
//             if (fullPath.isQualified() && currentPath != fullPath) {
//                 // We are dealing with a container-like path, ignore functions and variables
//                 if (!declaration->internalContext()
//                         || declaration->internalContext()->type() == DUContext::Other
//                         || declaration->internalContext()->type() == DUContext::Function) {
//                     continue;
//                 }
//             }
//
//             if (declaration->range() != useRange) {
//                 UseBuilderBase::newUse(useRange, DeclarationPointer(declaration));
//                 break;
//             }
//         }
//     }
// }

}
