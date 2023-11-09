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
#include <language/duchain/types/structuretype.h>
#include <language/duchain/types/functiontype.h>
#include <language/editor/documentrange.h>

#include "types/builtintype.h"
#include "types/slicetype.h"
#include "types/pointertype.h"
#include "types/optionaltype.h"
#include "expressionvisitor.h"
#include "zigdebug.h"
#include "helpers.h"


namespace Zig
{

using namespace KDevelop;

UseBuilder::UseBuilder(const KDevelop::IndexedString &document)
    : document(document)
{
}

VisitResult UseBuilder::visitNode(const ZigNode &node, const ZigNode &parent)
{
    NodeTag tag = node.tag();
    // qCDebug(KDEV_ZIG) << "UseBuilder::visitNode" << node.index << "tag" << tag;
    switch (tag) {
        case NodeTag_identifier:
            visitIdent(node, parent);
            break;
        case NodeTag_field_access:
            visitFieldAccess(node, parent);
            break;
        case NodeTag_call:
        case NodeTag_call_comma:
        case NodeTag_call_one:
        case NodeTag_call_one_comma:
        case NodeTag_async_call:
        case NodeTag_async_call_comma:
            visitCall(node, parent);
            break;
        case NodeTag_builtin_call:
        case NodeTag_builtin_call_comma:
        case NodeTag_builtin_call_two:
        case NodeTag_builtin_call_two_comma:
            visitBuiltinCall(node, parent);
            break;
        case NodeTag_struct_init:
        case NodeTag_struct_init_comma:
        case NodeTag_struct_init_one:
        case NodeTag_struct_init_one_comma:
            visitStructInit(node, parent);
            break;
        case NodeTag_deref:
            visitDeref(node, parent);
            break;
        case NodeTag_unwrap_optional:
            visitUnwrapOptional(node, parent);
            break;
        case NodeTag_array_access:
            visitArrayAccess(node, parent);
            break;
        // case VarAccess:
        //     visitVarAccess(node, parent);
        //     break;
        // case ArrayAccess:
        //     visitArrayAccess(node, parent);
        //     break;
        // case PtrAccess:
        //     visitPtrAccess(node, parent);
        //     break;
    }
    return ContextBuilder::visitNode(node, parent);
}

VisitResult UseBuilder::visitBuiltinCall(const ZigNode &node, const ZigNode &parent)
{
    QString functionName = node.spellingName();
    if (functionName == "@import") {
        // Show use range on the string
        ZigNode child = node.nextChild();
        QString importName = child.spellingName();
        RangeInRevision useRange = editorFindSpellingRange(child, importName);

        ExpressionVisitor v(session ,currentContext());
        v.startVisiting(node, parent);
        auto decl = v.lastDeclaration();
        if (decl) {
            if (decl->range() != useRange) {
                UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
            }
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
        return Continue;
    }
    else if (!BuiltinType::isBuiltinFunc(functionName)) {
        RangeInRevision useRange = editorFindSpellingRange(node, functionName);
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Error);
        p->setDescription(i18n("Undefined builtin %1", functionName));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return Continue;
    }
    // TODO: use for @This() ?
    // builtins have no decls
    return Continue;
}

VisitResult UseBuilder::visitCall(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    //// qDebug()<< "call" << functionName;
    ZigNode child = node.nextChild();
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(child, node);

    QString functionName = child.spellingName();
    RangeInRevision useRange = editorFindSpellingRange(node, functionName);

    if (auto fn = v.lastType().dynamicCast<FunctionType>()) {
        auto decl = v.lastDeclaration();
        if (decl && decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Undefined function"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitStructInit(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode owner = node.nextChild(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    auto useRange = editorFindSpellingRange(owner, owner.spellingName());
    if (auto s = v.lastType().dynamicCast<StructureType>()) {
        auto decl = v.lastDeclaration();
        if (decl && decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }

    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Undefined struct"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitVarAccess(const ZigNode &node, const ZigNode &parent)
{
    // TODO
    return Continue;
}

VisitResult UseBuilder::visitFieldAccess(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString attr = node.spellingName();
    if (attr.isEmpty()) {
        return Continue;
    }
    ZigNode owner = node.nextChild(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    const auto T = v.lastType();
    if (auto s = T.dynamicCast<SliceType>()) {
        if (attr == "len" || attr == "ptr") {
            return Continue; // Builtins
        }
    }

    auto *decl = Helper::accessAttribute(T, attr, v.lastTopContext());
    RangeInRevision useRange = editorFindSpellingRange(node, attr);
    if (!decl) {
        DUChainWriteLocker lock;
        QString ident = T ? T->toString() : "unknown"; // T toString needs lock
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("No field %1 on %2", attr, ident));
        topContext()->addProblem(p);
    }
    else if (decl->range() != useRange) {
        // qDebug() << "Create use:" << node.index << " name:" << attr << " range:" << useRange;
        UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
    }
    return Continue;
}

VisitResult UseBuilder::visitArrayAccess(const ZigNode &node, const ZigNode &parent)
{
    // TODO
    Q_UNUSED(parent);
    NodeData data = node.data(); // access lhs
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v1(session, currentContext());
    v1.startVisiting(lhs, node);
    ExpressionVisitor v2(session, currentContext());
    v2.startVisiting(rhs, node);

    if (auto slice = v1.lastType().dynamicCast<SliceType>()) {
        if (auto index = v2.lastType().dynamicCast<BuiltinType>()) {
            if (index->isInteger()) {
                auto decl = v1.lastDeclaration();
                auto useRange = lhs.range();
                if (decl && decl->range() != useRange) {
                    UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
                }
                return Continue;
            }
        }
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, rhs.range().castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Array index is not an integer type"));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
    } else {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, lhs.range().castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Attempt to index non-array type"));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
    }
    return Continue;
}

VisitResult UseBuilder::visitUnwrapOptional(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode owner = node.nextChild(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    auto T = v.lastType();
    if (auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
    }
    auto useRange = node.range();
    if (auto s = T.dynamicCast<OptionalType>()) {
        auto decl = Helper::declForIdentifiedType(s->baseType(), nullptr);
        if (decl && decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }

    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Attempt to unwrap non-optional type"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitDeref(const ZigNode &node, const ZigNode &parent)
{
    // TODO
    Q_UNUSED(parent);
    ZigNode owner = node.nextChild(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    auto useRange = node.range();
    if (auto ptr = v.lastType().dynamicCast<PointerType>()) {
        auto decl = Helper::declForIdentifiedType(ptr->baseType(), nullptr);
        if (decl && decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }

    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Attempt to dereference non-pointer type"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitIdent(const ZigNode &node, const ZigNode &parent)
{
    QString name = node.spellingName();

    if (
        name.isEmpty()
        || BuiltinType::isBuiltinVariable(name)
        || BuiltinType::isBuiltinType(name)
        || name == "." // TODO
    ) {
        return Continue;
    }
    RangeInRevision useRange = editorFindSpellingRange(node, name);
    Declaration* decl = Helper::declarationForName(
        name,
        useRange.start,
        DUChainPointer<const DUContext>(currentContext())
    );
    if (decl) {
        if (decl->range() != useRange) {
            // qDebug() << "Create use:" << node.index << " name:" << name << " range:" << useRange;
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }

    if (parent.kind() == FieldDecl) {
        // Enum field
        // TODO: Validate name ?
        return Continue;
    }

    qCDebug(KDEV_ZIG) << "Undefined variable " << name;
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Undefined variable %1", name));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

} // end namespace zig
