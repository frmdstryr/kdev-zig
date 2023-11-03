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

#include "types/builtintype.h"
#include "expressionvisitor.h"
#include "zigdebug.h"
#include "helpers.h"
#include "types/slicetype.h"

namespace Zig
{

using namespace KDevelop;

UseBuilder::UseBuilder(const KDevelop::IndexedString &document)
    : document(document)
{
}

VisitResult UseBuilder::visitNode(ZigNode &node, ZigNode &parent)
{
    NodeKind kind = node.kind();
    // qDebug() << "UseBuilder::visitNode" << node.index << "kind" << kind;
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
    //// qDebug()<< "call" << functionName;
    RangeInRevision useRange = editorFindSpellingRange(node, functionName);

    IndexedIdentifier identifier = IndexedIdentifier(Identifier(functionName));
    currentPath.clear();
    currentPath.push(identifier);
    if (functionName.isEmpty()) {
        return;
    }
    if (functionName.startsWith("@") && !BuiltinType::isBuiltinFunc(functionName))
    {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Error);
        p->setDescription(i18n("Undefined builtin %1", functionName));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return;
    }
    ZigNode child = node.nextChild();

    if (functionName == "@import") {
        // Show use range on the string
        QString importName = child.spellingName();
        useRange = editorFindSpellingRange(child, importName);
        ExpressionVisitor v(session ,currentContext());
        v.startVisiting(node, parent);
        auto mod = v.lastDeclaration();
        if (mod) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(mod));
        } else {
            ProblemPointer p = ProblemPointer(new Problem());

            p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
            p->setSource(IProblem::SemanticAnalysis);
            QUrl url = Helper::importPath(importName, session->document().str());
            if (url.isEmpty()) {
                p->setSeverity(IProblem::Warning);
                p->setDescription(i18n("Import \"%1\" does not exist", importName));
                if (importName.endsWith(".zig")) {
                    p->setExplanation(i18n("File not found"));
                } else {
                    p->setExplanation(i18n("Package path not defined"));
                }
            } else {
                p->setSeverity(IProblem::Hint);
                p->setDescription(i18n("Import %1 not yet resolved", importName));
                p->setExplanation(i18n("File at %1", url.toString()));

            }
            DUChainWriteLocker lock;
            topContext()->addProblem(p);
        }
        return;
    }

    QList<Declaration *> declarations;
    bool show_error = true;

    if (child.kind() == FieldAccess) {
        // TODO: Find type of child or use generic expression parser?
        ZigNode owner = child.nextChild();
        if (node.kind() == Ident) {
            DUChainReadLocker lock;
            QualifiedIdentifier ownerPath(Identifier(owner.spellingName()));
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

    if (declarations.isEmpty() && show_error && !functionName.startsWith("@")) {
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
                // qDebug() << "Create use:" << node.index << " name:" << containerName << " range:" << useRange;
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

    QString attr = node.spellingName();
    if (attr.isEmpty()) {
        return;
    }
    RangeInRevision useRange = editorFindSpellingRange(node, attr);

    QualifiedIdentifier identifier((Identifier(attr)));
    ZigNode owner = node.nextChild(); // access lhs
    DUChainReadLocker lock;
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    const auto T = v.lastType();
    QString ident = T ? T->toString() : "unknown";

    if (auto s = T.dynamicCast<SliceType>()) {
        if (attr == "len" || attr == "ptr") {
            return; // Builtins
        }
    }

    auto *decl = Helper::accessAttribute(T, attr, topContext());
    lock.unlock();

    if (!decl) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("No field %1 on %2", attr, ident));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
    }
    else if (decl->range() != useRange) {
        // qDebug() << "Create use:" << node.index << " name:" << attr << " range:" << useRange;
        UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
    }
}

void UseBuilder::visitArrayAccess(ZigNode &node, ZigNode &parent)
{
    // TODO
    Q_UNUSED(parent);
}

void UseBuilder::visitPtrAccess(ZigNode &node, ZigNode &parent)
{
    // TODO
    Q_UNUSED(parent);
}

void UseBuilder::visitIdent(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.spellingName();
    RangeInRevision useRange = editorFindSpellingRange(node, name);

    IndexedIdentifier identifier = IndexedIdentifier(Identifier(name));
    currentPath.clear();
    currentPath.push(identifier);
    if (name.isEmpty()) {
        return;
    }
    if (name == ".") {
        return;  // TODO: Handle .
    }

    QList<Declaration *> declarations;
    {
        DUChainReadLocker lock;
        DUContext *context = topContext()->findContextAt(useRange.start);
        declarations = context->findLocalDeclarations(identifier);
        if (declarations.isEmpty()) {
            QualifiedIdentifier qualifiedIdentifier((Identifier(name)));
            declarations = findSimpleVar(qualifiedIdentifier, context, DUContext::NoSearchFlags);
        }
    }

    if (declarations.isEmpty()
        && !BuiltinType::isBuiltinVariable(name) // TODO: Check depending on usage
        && !BuiltinType::isBuiltinType(name) // TODO: Check depending on usage
    ) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Undefined variable %1", name));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
    } else {
        for (Declaration *declaration : declarations) {
            // TODO: Check if a struct or alias of struct
            if (declaration->range() != useRange) {
                // qDebug() << "Create use:" << node.index << " name:" << name << " range:" << useRange;
                UseBuilderBase::newUse(useRange, DeclarationPointer(declaration));
                break;
            }
        }
    }
}

void UseBuilder::visitLiteral(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    // TODO
}

}
