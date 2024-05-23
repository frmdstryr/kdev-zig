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
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/classmemberdeclaration.h>
#include <language/duchain/problem.h>
#include <language/duchain/types/structuretype.h>
#include <language/duchain/types/functiontype.h>
#include <language/editor/documentrange.h>

#include "types/builtintype.h"
#include "types/slicetype.h"
#include "types/pointertype.h"
#include "types/optionaltype.h"
#include "types/delayedtype.h"
#include "types/enumtype.h"
#include "types/uniontype.h"
#include "types/errortype.h"
#include "types/vectortype.h"

#include "expressionvisitor.h"
#include "zigdebug.h"
#include "helpers.h"
#include "delayedtypevisitor.h"

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
        case NodeTag_struct_init_dot:
        case NodeTag_struct_init_dot_comma:
        case NodeTag_struct_init_dot_two:
        case NodeTag_struct_init_dot_two_comma:
            visitStructInitDot(node, parent);
            break;
        case NodeTag_array_init:
        case NodeTag_array_init_comma:
        case NodeTag_array_init_one:
        case NodeTag_array_init_one_comma:
            visitArrayInit(node, parent);
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
        case NodeTag_if:
        case NodeTag_if_simple:
            visitIf(node, parent);
            break;
        case NodeTag_enum_literal:
            visitEnumLiteral(node, parent);
            break;
        case NodeTag_switch:
        case NodeTag_switch_comma:
            visitSwitch(node, parent);
            break;
        case NodeTag_switch_case:
        case NodeTag_switch_case_inline:
        case NodeTag_switch_case_one:
        case NodeTag_switch_case_inline_one:
            visitSwitchCase(node, parent);
            break;
        case NodeTag_assign:
            visitAssign(node, parent);
            break;
        case NodeTag_try:
            visitTry(node, parent);
            break;
        case NodeTag_catch:
            visitCatch(node, parent);
            break;
        case NodeTag_container_field:
        case NodeTag_container_field_align:
        case NodeTag_container_field_init:
            return visitContainerField(node, parent);
        // TODO: check var_init and field_init values match type

        default:
            break;
    }
    return ContextBuilder::visitNode(node, parent);
}


VisitResult UseBuilder::visitContainerField(const ZigNode &node, const ZigNode &parent)
{
    const QString fieldName = node.spellingName();
    auto range = node.spellingRange();
    const auto previousField = currentFieldDeclaration;
    currentFieldDeclaration = Helper::declarationForName(
        fieldName,
        range.start,
        DUChainPointer<const DUContext>(currentContext()),
        previousField
    );
    visitChildren(node, parent);
    currentFieldDeclaration = previousField;
    return VisitResult::Continue;
}

VisitResult UseBuilder::visitBuiltinCall(const ZigNode &node, const ZigNode &parent)
{
    QString functionName = node.spellingName();
    if (functionName == QLatin1String("@import")) {
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
                if (importName.endsWith(QStringLiteral(".zig"))) {
                    p->setExplanation(i18n("File not found"));
                } else {
                    p->setExplanation(i18n("Package path not defined"));
                }
            } else {
                p->setSeverity(IProblem::Hint);
                p->setDescription(i18n("Import %1 not yet resolved", importName));
                p->setExplanation(i18n("Located at %1", url.toString()));

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
    RangeInRevision useRange = editorFindSpellingRange(child, functionName);
    auto fn = v.lastType().dynamicCast<FunctionType>();
    if (!fn) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Undefined function"));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return Continue;
    }

    // TODO: It may be a a renamed or extern fn that the visitor
    if (auto decl = v.lastDeclaration().dynamicCast<FunctionDeclaration>()) {
        if (decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
    }

    const auto n = node.callParamCount();
    const auto args = fn->arguments();

    // Handle implict "self" arg in struct fns
    int startArg = 0;
    if (args.size() > 0) {
        AbstractType::Ptr selfType  = v.functionCallSelfType(child, node);
        if (Helper::baseTypesEqual(args.at(0), selfType)) {
            startArg = 1;
        }
    }
    const auto requiredArgs = static_cast<uint32_t>(args.size() - startArg);

    // Instead of throwing an error for any mismatch like what zig does
    // still check types for whatever we can
    if (n == 0 && args.size() > startArg) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, node.mainTokenRange().castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
        if (requiredArgs == 1) {
            p->setDescription(i18n("Expected 1 argument"));
        } else {
            p->setDescription(i18n("Expected %1 arguments", requiredArgs));
        }
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return Continue;
    }

    // Check fn arguments
    uint32_t i = 0;
    QMap<IndexedString, AbstractType::Ptr> resolvedArgTypes;
    for (const auto &arg: args.mid(startArg)) {
        ZigNode argValueNode = node.callParamAt(i);
        checkAndAddFnArgUse(arg, i, argValueNode, node, resolvedArgTypes);
        i += 1;
    }

    if (n > requiredArgs) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, node.mainTokenRange().castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
        const auto extra = n - requiredArgs;
        if (extra == 1) {
            p->setDescription(i18n("Function has an extra argument"));
        } else {
            p->setDescription(i18n("Function has %1 extra arguments", extra));
        }
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
    }
    auto returnType = Helper::asZigType(fn->returnType());
    if (auto errorType = returnType.dynamicCast<ErrorType>()) {
        returnType = errorType->baseType();
        switch (parent.tag()) {
            case NodeTag_block:
            case NodeTag_block_semicolon:
            case NodeTag_block_two:
            case NodeTag_block_two_semicolon: {
                ProblemPointer p = ProblemPointer(new Problem());
                p->setFinalLocation(DocumentRange(document, node.mainTokenRange().castToSimpleRange()));
                p->setSource(IProblem::SemanticAnalysis);
                p->setSeverity(IProblem::Warning);
                p->setDescription(i18n("Error is ignored"));
                DUChainWriteLocker lock;
                topContext()->addProblem(p);
                break;
            }
            default:
                break;
        }
    }

     switch (parent.tag()) {
        case NodeTag_block:
        case NodeTag_block_semicolon:
        case NodeTag_block_two:
        case NodeTag_block_two_semicolon: {
            auto builtin = returnType.dynamicCast<BuiltinType>();
            if (builtin && (builtin->isVoid() || builtin->isNoreturn()) ) {
                // ok
            } else if (Helper::isMixedType(returnType)) {
                // maybe an error, idk
            } else {
                ProblemPointer p = ProblemPointer(new Problem());
                p->setFinalLocation(DocumentRange(document, node.mainTokenRange().castToSimpleRange()));
                p->setSource(IProblem::SemanticAnalysis);
                p->setSeverity(IProblem::Warning);
                p->setDescription(i18n("Return value is ignored"));
                DUChainWriteLocker lock;
                topContext()->addProblem(p);
            }
        }
        default:
            break;
     }


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
        checkAndAddStructInitUse(s, node, useRange);
    }
    // TODO: else error ??
    return Continue;
}

VisitResult UseBuilder::visitStructInitDot(const ZigNode &node, const ZigNode &parent)
{
    ZigNode typeNode = parent.varType();
    if (!typeNode.isRoot()) {
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(typeNode, parent);

        auto useRange = editorFindSpellingRange(typeNode, parent.spellingName());
        if (auto s = v.lastType().dynamicCast<StructureType>()) {
            checkAndAddStructInitUse(s, node, useRange);
        }
        // TODO: else error ??
    }
    return Continue;
}

AssignmentCheckResult UseBuilder::checkGenericAssignment(
    const KDevelop::AbstractType::Ptr &lhs,
    const ZigNode &node,
    const ZigNode &parent)
{
    auto target = lhs;
    if (auto opt = target.dynamicCast<OptionalType>()) {
        target = opt->baseType();
    }
    NodeTag tag = node.tag();
    if (auto enumeration = target.dynamicCast<EnumType>()) {
        if (tag == NodeTag_enum_literal) {
            bool ok = checkAndAddEnumUse(
                enumeration,
                node.mainToken(),
                node.mainTokenRange()
            );
            return {ok, false, enumeration};
        }
    }

    if (auto s = target.dynamicCast<StructureType>()) {
        if (tag == NodeTag_struct_init_dot
            || tag == NodeTag_struct_init_dot_comma
            || tag == NodeTag_struct_init_dot_two
            || tag == NodeTag_struct_init_dot_two_comma
        ) {
            TokenIndex tok = ast_node_main_token(node.ast, node.index);
            auto dotRange = node.tokenRange(tok - 1);
            bool ok = checkAndAddStructInitUse(s, node, dotRange);
            return {ok, false, s};
        }
    }

    if (auto s = target.dynamicCast<SliceType>()) {
        if (tag == NodeTag_struct_init_dot
            || tag == NodeTag_array_init_dot
            || tag == NodeTag_array_init_dot_comma
            || tag == NodeTag_array_init_dot_two
            || tag == NodeTag_array_init_dot_two_comma
        ) {
            TokenIndex tok = ast_node_main_token(node.ast, node.index);
            auto dotRange = node.tokenRange(tok - 1);
            bool ok = checkAndAddArrayInitUse(s, node, dotRange);
            return {ok, false, s};
        }
    }

    ExpressionVisitor v(session, currentContext());
    v.setInferredType(target);
    v.startVisiting(node, parent);
    bool ok = Helper::canTypeBeAssigned(lhs, v.lastType());
    return {ok, !ok, v.lastType()};
}

bool UseBuilder::checkAndAddStructInitUse(
    const KDevelop::StructureType::Ptr &structType,
    const ZigNode &structInitNode,
    const KDevelop::RangeInRevision &useRange)
{
    Declaration* decl;
    {
        DUChainReadLocker lock;
        decl = structType->declaration(nullptr);
    }
    if (!decl) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Undefined struct"));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return false;
    }
    if (decl->range() != useRange) {
        UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
    }
    auto unionType = decl->type<UnionType>();
    // TODO: Check fields
    const auto n = structInitNode.structInitCount();

    bool ok = true;
    for (uint32_t i=0; i < n; i++) {
        FieldInitData fieldData = structInitNode.structInitAt(i);
        ZigNode fieldValue = {structInitNode.ast, fieldData.value_expr};
        QString fieldName = structInitNode.tokenSlice(fieldData.name_token);
        auto useRange = structInitNode.tokenRange(fieldData.name_token);
        if (!checkAndAddStructFieldUse(structType, fieldName, fieldValue, structInitNode, useRange)) {
            ok = false;
        }

        if (unionType && i > 1) {
            ProblemPointer p = ProblemPointer(new Problem());
            p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
            p->setSource(IProblem::SemanticAnalysis);
            p->setSeverity(IProblem::Hint);
            p->setDescription(i18n("Union can only have one field"));
            DUChainWriteLocker lock;
            topContext()->addProblem(p);
            ok = false;
            break; // Show for all ?
        }
    }
    return ok;
}

bool UseBuilder::checkAndAddStructFieldUse(
        const KDevelop::StructureType::Ptr &structType,
        const QString &fieldName,
        const ZigNode &valueNode,
        const ZigNode &structInitNode,
        const KDevelop::RangeInRevision &useRange)
{
    auto decl = Helper::accessAttribute(structType, fieldName, topContext());
    if (!decl) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
        DUChainWriteLocker lock;
        p->setDescription(
            i18n("Struct %1 has no field %2", structType->toString(), fieldName));
        topContext()->addProblem(p);
        return false;
    }

    if (decl->range() != useRange) {
        UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
    }

    auto target = Helper::asZigType(decl->abstractType());
    if (Helper::isMixedType(target)) {
        return false;
    }
    if (auto unionType = decl->type<UnionType>()) {
        if (unionType->dataType()) {
            target = unionType->dataType();
        }
    }
    auto result = checkGenericAssignment(target, valueNode, structInitNode);
    if (result.mismatch) {
        if (Helper::isMixedType(result.value)) {
            return false;
        }
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
        DUChainWriteLocker lock;
        p->setDescription(i18n(
            "Struct field type mismatch. Expected %1 got %2",
            decl->abstractType()->toString(), result.value->toString()));
        topContext()->addProblem(p);
        return false;

    }
    return true;
}

VisitResult UseBuilder::visitArrayInit(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode owner = node.nextChild(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    auto useRange = editorFindSpellingRange(owner, owner.spellingName());
    if (auto s = v.lastType().dynamicCast<SliceType>()) {
        checkAndAddArrayInitUse(s, node, useRange);
    }
    // TODO: else error ??
    return Continue;

}


bool UseBuilder::checkAndAddArrayInitUse(
        const SliceType::Ptr &sliceType,
        const ZigNode &arrayInitNode,
        const KDevelop::RangeInRevision &useRange)
{
    Q_UNUSED(useRange);
    if (Helper::isMixedType(sliceType->elementType())) {
        return true; // No point in checking
    }
    const auto n = arrayInitNode.arrayInitCount();
    bool ok = true;
    for (uint32_t i=0; i < n; i++) {
        ZigNode valueNode = arrayInitNode.arrayInitAt(i);
        Q_ASSERT(!valueNode.isRoot());
        auto itemRange = valueNode.spellingRange();
        if (!checkAndAddArrayItemUse(sliceType->elementType(), i, valueNode, arrayInitNode, itemRange)) {
            ok = false;
        }
    }
    return ok;
}

bool UseBuilder::checkAndAddArrayItemUse(
        const KDevelop::AbstractType::Ptr &itemType,
        const uint32_t itemIndex,
        const ZigNode &valueNode,
        const ZigNode &arrayInitNode,
        const KDevelop::RangeInRevision &useRange)
{
    auto result = checkGenericAssignment(itemType, valueNode, arrayInitNode);
    if (result.mismatch) {
        if (Helper::isMixedType(result.value)) {
            return true; // Probably not implemented or resolved yet
        }
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
        DUChainWriteLocker lock;
        p->setDescription(i18n(
            "Array item type mismatch at index %1. Expected %2 got %3",
            itemIndex, itemType->toString(), result.value->toString()));
        topContext()->addProblem(p);
    }
    return true;
}

VisitResult UseBuilder::visitAssign(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    const NodeTag tag = lhs.tag();
    if (tag == NodeTag_identifier) {
        if (lhs.mainToken() == QLatin1String("_")) {
            // TODO: Check if it is used ?
            return Continue; // Discard
        }
    }

    ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(lhs, node);
    auto target =  Helper::asZigType(v.lastType());
    if (Helper::isMixedType(target)) {
        return Continue; // Probably not implemented or resolved yet
    }
    auto result = checkGenericAssignment(target, rhs, node);
    if (result.mismatch) {
        if (Helper::isMixedType(result.value)) {
            return Continue;
        }
        auto useRange = (tag == NodeTag_field_access) ? node.tokenRange(lhs.data().rhs) : lhs.range();
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
        DUChainWriteLocker lock;
        p->setDescription(i18n(
            "Assignment type mismatch. Expected %1 got %2",
            target->toString(), result.value->toString()));
        topContext()->addProblem(p);
    }
    return Continue;
}


VisitResult UseBuilder::visitFieldAccess(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString attr = node.spellingName();
    if (attr.isEmpty()) {
        return Continue;
    }
    ZigNode owner = node.lhsAsNode(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.setExcludedDeclaration(currentFieldDeclaration);
    v.startVisiting(owner, node);
    auto T = Helper::asZigType(v.lastType());
    if (auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
    }
    if (auto s = T.dynamicCast<SliceType>()) {
        if (attr == QLatin1String("len") || attr == QLatin1String("ptr")) {
            return Continue; // Builtins
        }
    }

    if (Helper::isMixedType(T)) {
        return Continue; // Ignore
    }

    if (T.dynamicCast<DelayedType>()) {
        return Continue; // Ignore unresolved comptime types.
    }

    auto *decl = Helper::accessAttribute(T, attr, topContext());
    RangeInRevision useRange = editorFindSpellingRange(node, attr);
    if (!decl) {
        DUChainWriteLocker lock;
        QString ident = T ? T->toString() : QStringLiteral("unknown"); // T toString needs lock
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Warning);
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
    ExpressionVisitor v1(session, currentContext());
    v1.startVisiting(lhs, node);
    auto T = v1.lastType();

    bool is_c_ptr = false;
    if (auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
        is_c_ptr = (ptr->modifiers() & ArrayModifier) != 0;
    }
    if (T.dynamicCast<SliceType>() || T.dynamicCast<VectorType>() || is_c_ptr) {
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(session, currentContext());
        v2.setInferredType(BuiltinType::newFromName(QStringLiteral("usize")));
        v2.startVisiting(rhs, node);
        if (auto index = v2.lastType().dynamicCast<BuiltinType>()) {
            // TODO: Check range if known ?
            if (index->isInteger()) {
                auto decl = v1.lastDeclaration(); // Is this correct ?
                auto useRange = lhs.spellingRange();
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
        p->setFinalLocation(DocumentRange(document, lhs.spellingRange().castToSimpleRange()));
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
    ZigNode lhs = node.lhsAsNode(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(lhs, node);
    auto T = v.lastType();
    if (auto ptr = T.dynamicCast<PointerType>())
        T = ptr->baseType();
    if (Helper::isMixedType(T))
        return Continue; // Maybe error, ignore

    auto useRange = node.range();
    if (auto s = T.dynamicCast<OptionalType>()) {
        auto decl = Helper::declarationForIdentifiedType(s->baseType(), nullptr);
        if (decl && decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }

    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Warning);
    p->setDescription(i18n("Attempt to unwrap non-optional type"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitTry(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode lhs = node.lhsAsNode(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(lhs, node);
    if (Helper::isMixedType(v.lastType()))
        return Continue; // May be an error, idk
    if (auto errorType = v.lastType().dynamicCast<ErrorType>())
        return Continue;

    auto useRange = lhs.range();
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Warning);
    p->setDescription(i18n("Try on non-error type"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitCatch(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode lhs = node.lhsAsNode(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(lhs, node);
    if (Helper::isMixedType(v.lastType()))
        return Continue; // May be an error, idk
    if (auto errorType = v.lastType().dynamicCast<ErrorType>()) {

        if (Helper::isMixedType(errorType->baseType())) {
            return Continue;
        }

        ExpressionVisitor v2(session, currentContext());
        v2.startVisiting(node.rhsAsNode(), node);
        if (Helper::isMixedType(v2.lastType())) {
            return Continue;
        }
        if (auto builtin = v2.lastType().dynamicCast<BuiltinType>()) {
            if (builtin->isTrap()) {
                return Continue; // Ignore foo() catch @panic()
            }
        }


        auto targetType = errorType->baseType();
        // TODO: Determining whether the target is const should be done better,
        // this only works for a single case of const foo = a catch b;
        if (parent.kind() == VarDecl && parent.mainToken() == QStringLiteral("const")) {
            targetType->setModifiers(targetType->modifiers() | AbstractType::ConstModifier);
        }

        if (!Helper::canTypeBeAssigned(targetType, v2.lastType())) {
            if (v2.returnType()) {
                // Eg catch with return block
                // TODO: Should check fn return is correct type?
                return Continue;
            }
            auto useRange = node.range();
            ProblemPointer p = ProblemPointer(new Problem());
            p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
            p->setSource(IProblem::SemanticAnalysis);
            p->setSeverity(IProblem::Warning);
            DUChainWriteLocker lock;
            p->setDescription(i18n("Incompatible types %1 and %2",
                errorType->baseType()->toString(),
                v2.lastType()->toString()
            ));
            topContext()->addProblem(p);
        }
        return Continue;
    }

    auto useRange = lhs.range();
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Warning);
    p->setDescription(i18n("Catch on non-error type"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}


VisitResult UseBuilder::visitDeref(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode owner = node.lhsAsNode(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(owner, node);
    if (Helper::isMixedType(v.lastType()))
        return Continue; // May be an error, idk
    auto useRange = node.range();
    auto T = Helper::asZigType(v.lastType());
    if (auto ptr = T.dynamicCast<PointerType>()) {
        auto decl = Helper::declarationForIdentifiedType(ptr->baseType(), nullptr);
        if (decl && decl->range() != useRange) {
            UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
        }
        return Continue;
    }

    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Warning);
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
        || name == QLatin1String("_")
        || name == QLatin1String(".") // TODO
    ) {
        return Continue;
    }
    RangeInRevision useRange = editorFindSpellingRange(node, name);
    Declaration* decl = Helper::declarationForName(
        name,
        useRange.start,
        DUChainPointer<const DUContext>(currentContext()),
        currentFieldDeclaration
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

    // qCDebug(KDEV_ZIG) << "Undefined variable " << name;
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Undefined variable %1", name));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitIf(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode cond = node.nextChild(); // access lhs
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(cond, node);
    QString capture = node.captureName(CaptureType::Payload);
    if (capture.isEmpty() && !Helper::isMixedType(v.lastType())) {
        if (auto builtin = v.lastType().dynamicCast<BuiltinType>()) {
            if (builtin->isBool()) {
                return Continue; // Ok
            }
        }
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, cond.range().castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        if (auto optional = v.lastType().dynamicCast<OptionalType>()) {
            p->setSeverity(IProblem::Warning);
            p->setDescription(i18n("Used if on optional type with no capture or comparison"));
        } else {
            p->setSeverity(IProblem::Hint);
            p->setDescription(i18n("if condition is not a bool"));
        }
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return Continue;

    }
    // Error for the non optional case is handled in the declaration builder
    // when creating the capture decl
    return Continue;
}

VisitResult UseBuilder::visitEnumLiteral(const ZigNode &node, const ZigNode &parent)
{
    NodeKind parentKind = parent.kind();
    if (parentKind == VarDecl || parentKind == FieldDecl) {
        QString enumName = node.mainToken();
        auto useRange = node.mainTokenRange();
        auto target = Helper::declarationForName(
            parent.spellingName(),
            useRange.start,
            DUChainPointer<const DUContext>(currentContext())
        );
        if (!target) {
            // TODO: Report error?
            return Continue;
        }
        checkAndAddEnumUse(target->abstractType(), node.mainToken(), node.mainTokenRange());
        return Continue;
    }

    NodeTag tag = parent.tag();
    if (tag == NodeTag_equal_equal || tag == NodeTag_bang_equal) {
        NodeData data = parent.data();
        ZigNode lhs = {parent.ast, data.lhs};
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(lhs, parent);
        checkAndAddEnumUse(v.lastType(), node.mainToken(), node.mainTokenRange());
        return Continue;
    }
    // if (tag == NodeTag_array_init) {
    //       // TODO: Cache parent type somehow
    //     ZigNode arrayType = {parent.ast, parent.data().lhs};
    //     ZigNode typeNode = {parent.ast, arrayType.data().rhs};
    //     ExpressionVisitor v(session, currentContext()->parentContext());
    //     v.startVisiting(typeNode, node);
    //     checkAndAddEnumUse(v.lastType(), node.mainToken(), node.mainTokenRange());
    // }


    // TODO: Switch, fn values params

    // ProblemPointer p = ProblemPointer(new Problem());
    // p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    // p->setSource(IProblem::SemanticAnalysis);
    // p->setSeverity(IProblem::Hint);
    // p->setDescription(i18n("Attempt to dereference non-pointer type"));
    // DUChainWriteLocker lock;
    // topContext()->addProblem(p);
    return Continue;
}



VisitResult UseBuilder::visitSwitch(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode lhs = {node.ast, node.data().lhs};
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(lhs, node);

    if (auto builtin = v.lastType().dynamicCast<BuiltinType>()) {
        if (builtin->isInteger() || builtin->isBool()) {
            return Continue; // No problem
        }
    }
    else if (auto enumeration = v.lastType().dynamicCast<EnumType>()) {
        return Continue; // No problem
    }
    else if (auto unionType = v.lastType().dynamicCast<UnionType>()) {
        if (unionType->isEnum())
            return Continue; // No problem
    }
    // TODO: What else can switch?
    // TODO: Check if all cases handled ?

    auto useRange = lhs.range();
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Switch on invalid type"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;
}

VisitResult UseBuilder::visitSwitchCase(const ZigNode &node, const ZigNode &parent)
{
    ZigNode switchTypeNode = {parent.ast, parent.data().lhs};
    ExpressionVisitor v(session, currentContext()->parentContext());
    v.startVisiting(switchTypeNode, node);
    const auto switchType = v.lastType();

    const auto n = node.switchCaseCount();
    for (uint32_t i=0; i < n; i++) {
        ZigNode item = node.switchCaseItemAt(i);
        if (item.tag() == NodeTag_enum_literal) {
            checkAndAddEnumUse(switchType, item.mainToken(), item.mainTokenRange());
        }
    }
    return Continue;
}


bool UseBuilder::checkAndAddFnArgUse(
        const KDevelop::AbstractType::Ptr &argType,
        const uint32_t argIndex,
        const ZigNode &argValueNode,
        const ZigNode &callNode,
        QMap<KDevelop::IndexedString, KDevelop::AbstractType::Ptr> &resolvedArgTypes)
{
    // {
    //     DUChainReadLocker lock;
    //     qCDebug(KDEV_ZIG) << "Checking arg " << argIndex << argType->toString();
    // }

    if (argValueNode.isRoot()) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, callNode.mainTokenRange().castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Argument %1 is missing", argIndex + 1));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return false;
    }

    // Resolve comptime type param
    if (auto templateParam = argType.dynamicCast<DelayedType>()) {
        // TODO: Check if argument is instance or type ?
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(argValueNode, callNode);
        resolvedArgTypes.insert(templateParam->identifier(), v.lastType());
        return true;
    }
    auto resolvedType = argType;
    if (resolvedArgTypes.size() > 0) {
        DelayedTypeFinder finder;
        argType->accept(&finder);
        if (finder.delayedTypes.size() > 0) {
            // This parameter depends on a delayed type
            for (const auto &t : finder.delayedTypes) {
                Q_ASSERT(!t->identifier().isEmpty());
                auto it = resolvedArgTypes.constFind(t->identifier());
                if (it != resolvedArgTypes.constEnd()) {
                    SimpleTypeExchanger exchanger(t, *it);
                    resolvedType = exchanger.exchange(AbstractType::Ptr(resolvedType->clone()));
                }
            }
        }
    }

    auto result = checkGenericAssignment(resolvedType, argValueNode, callNode);
    if (result.mismatch) {
        if (Helper::isMixedType(resolvedType)) {
            return false; // Don't show error if we don't know the target type
        }
        auto useRange = argValueNode.range();
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        DUChainWriteLocker lock;
        p->setDescription(i18n("Argument %1 type mismatch. Expected %2 got %3", argIndex + 1,
                            resolvedType->toString(), result.value->toString()));
        topContext()->addProblem(p);
    }
    return true;
}

bool UseBuilder::checkAndAddEnumUse(
        const KDevelop::AbstractType::Ptr &accessed,
        const QString &enumName,
        const KDevelop::RangeInRevision &useRange)
{
    AbstractType::Ptr enumType(accessed.dynamicCast<EnumType>());
    if (!enumType.data()) {
        if (auto unionType = accessed.dynamicCast<UnionType>()) {
            enumType = unionType->enumType();
        }
    }

    if (!enumType.data()) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Attempted to access enum field on non-enum type"));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
        return false;
    }

    auto decl = Helper::accessAttribute(enumType, enumName, topContext());
    // TODO: Verify decl is an enumeration type and not a fn or something
    if (!decl) {
        ProblemPointer p = ProblemPointer(new Problem());
        p->setFinalLocation(DocumentRange(document, useRange.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        DUChainWriteLocker lock;
        p->setDescription(i18n("Invalid enum field %1 on %2", enumName, enumType->toString()));
        topContext()->addProblem(p);
        return false;
    }
    if (decl->range() != useRange) {
        UseBuilderBase::newUse(useRange, DeclarationPointer(decl));
    }
    return true;
}

} // end namespace zig
