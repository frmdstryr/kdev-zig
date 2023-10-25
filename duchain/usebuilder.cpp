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

VisitResult UseBuilder::visitNode(ZigNode &node, ZigNode &parent)
{
    NodeKind kind = ast_node_kind(node.ast, node.index);
    NodeKind parentKind = ast_node_kind(parent.ast, parent.index);

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

void UseBuilder::visitCall(ZigNode &node, ZigNode &parent)
{
    // TODO
    QString functionName = node.spellingName();
    RangeInRevision useRange = editorFindSpellingRange(node, functionName);

    IndexedIdentifier identifier = IndexedIdentifier(Identifier(functionName));
    currentPath.clear();
    currentPath.push(identifier);
    if (functionName.isEmpty()) {
        return;
    }

    ZigNode child = node.nextChild();

    QList<Declaration *> declarations;
    bool show_error = true;

    if (child.kind() == FieldAccess) {
        // TODO: Find type of child or use generic expression parser?
        ZigNode owner = child.nextChild();
        if (node.kind() == Ident) {
            QualifiedIdentifier ownerPath(Identifier(owner.spellingName()));
            DUChainReadLocker lock;
            DUContext* context = topContext()->findContextAt(useRange.start);
            declarations = findSimpleVar(ownerPath, context);
            if (!declarations.isEmpty()) {
                DUContext* ownerContext = declarations.first()->internalContext();
                declarations = findSimpleVar(currentPath, ownerContext, DUContext::OnlyFunctions);
            }
        } else {
            show_error = false; // Might be a problem, IDK
        }
    } else {
        // Look for fn in scope
        DUChainReadLocker lock;
        DUContext *context = topContext()->findContextAt(useRange.start);
        declarations = findSimpleVar(currentPath, context, DUContext::OnlyFunctions);
    }

    if (declarations.isEmpty() && show_error) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Undefined function %1", functionName));
        DUChainWriteLocker lock;
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

void UseBuilder::visitContainerInit(ZigNode &node, ZigNode &parent)
{
    QString containerName = node.spellingName();
    RangeInRevision useRange = editorFindSpellingRange(node, containerName);

    IndexedIdentifier identifier = IndexedIdentifier(Identifier(containerName));
    currentPath.clear();
    currentPath.push(identifier);
    if (containerName.isEmpty()) {
        return;
    }
    if (containerName == ".") {
        return;  // TODO: Handle .
    }

    QList<Declaration *> declarations;
    {
        DUChainReadLocker lock;
        DUContext *context = topContext()->findContextAt(useRange.start);
        declarations = findSimpleVar(currentPath, context, DUContext::OnlyContainerTypes);
    }

    if (declarations.isEmpty()) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Undefined container %1", containerName));
        DUChainWriteLocker lock;
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

void UseBuilder::visitVarAccess(ZigNode &node, ZigNode &parent)
{
    // TODO
}

void UseBuilder::visitFieldAccess(ZigNode &node, ZigNode &parent)
{
    // TODO
}

void UseBuilder::visitArrayAccess(ZigNode &node, ZigNode &parent)
{
    // TODO
}

void UseBuilder::visitPtrAccess(ZigNode &node, ZigNode &parent)
{
    // TODO
}

void UseBuilder::visitIdent(ZigNode &node, ZigNode &parent)
{
    // TODO
}

void UseBuilder::visitLiteral(ZigNode &node, ZigNode &parent)
{
    // TODO
}

}
