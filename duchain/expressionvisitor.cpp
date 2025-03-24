#include <language/duchain/types/structuretype.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/typesystem.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/duchain.h>
#include <language/duchain/functiondeclaration.h>

#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/errortype.h"
#include "types/slicetype.h"
#include "types/comptimetype.h"
#include "types/enumtype.h"
#include "types/uniontype.h"
#include "types/vectortype.h"

#include "functionvisitor.h"
#include "delayedtypevisitor.h"
#include "expressionvisitor.h"

#include "helpers.h"
#include "zigdebug.h"
#include "nodetraits.h"


namespace Zig
{

static VisitResult expressionVisitorCallback(ZAst* ast, NodeIndex node, NodeIndex parent, void *data)
{
    ExpressionVisitor *visitor = static_cast<ExpressionVisitor *>(data);
    Q_ASSERT(visitor);
    ZigNode childNode = {ast, node};
    ZigNode parentNode = {ast, parent};
    return visitor->visitNode(childNode, parentNode);
}


ExpressionVisitor::ExpressionVisitor(ParseSession* session, const KDevelop::DUContext* context)
    : DynamicLanguageExpressionVisitor(context)
    , m_session(session)
{
    Q_ASSERT(m_session);
}

ExpressionVisitor::ExpressionVisitor(ExpressionVisitor* parent, const KDevelop::DUContext* overrideContext)
    : DynamicLanguageExpressionVisitor(parent)
    , m_session(parent->session())
    , m_excludedDeclaration(parent->excludedDeclaration())
{
    if ( overrideContext ) {
        m_context = overrideContext;
    }
    Q_ASSERT(context());
    Q_ASSERT(session());
}


void ExpressionVisitor::visitChildren(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ast_visit(node.ast, node.index, expressionVisitorCallback, this);
}

void ExpressionVisitor::startVisiting(const ZigNode &node, const ZigNode &parent)
{
    if (node.isRoot()) {
        return;
    }
    // if (auto cachedType = session()->typeFromNode(node)) {
    //     if (auto cachedDecl = session()->declFromNode(node)) {
    //         encounterLvalue(DeclarationPointer(cachedDecl));
    //     } else {
    //         encounter(cachedType);
    //     }
    //     return;
    // }
    visitNode(node, parent);
}

VisitResult ExpressionVisitor::visitNode(const ZigNode &node, const ZigNode &parent)
{
    NodeTag tag = node.tag();
    // qCDebug(KDEV_ZIG) << "ExpressionVisitor::visitNode" << node.index << "tag" << tag;
    switch (tag) {
    case NodeTag_identifier:
        return visitIdentifier(node, parent);
    case NodeTag_field_access:
        return visitFieldAccess(node, parent);
    case NodeTag_optional_type:
        return visitOptionalType(node, parent);
    case NodeTag_char_literal:
        return visitCharLiteral(node, parent);
    case NodeTag_string_literal:
        return visitStringLiteral(node, parent);
    case NodeTag_number_literal:
        return visitNumberLiteral(node, parent);
    case NodeTag_multiline_string_literal:
        return visitMultilineStringLiteral(node, parent);
    case NodeTag_enum_literal:
        return visitEnumLiteral(node, parent);
    case NodeTag_ptr_type:
    case NodeTag_ptr_type_bit_range:
    case NodeTag_ptr_type_aligned:
    case NodeTag_ptr_type_sentinel:
        return visitPointerType(node, parent);
    case NodeTag_container_decl:
    case NodeTag_container_decl_trailing:
    case NodeTag_container_decl_arg:
    case NodeTag_container_decl_arg_trailing:
    case NodeTag_container_decl_two:
    case NodeTag_container_decl_two_trailing:
        return visitContainerDecl(node, parent);
    case NodeTag_struct_init:
    case NodeTag_struct_init_comma:
    case NodeTag_struct_init_one:
    case NodeTag_struct_init_one_comma:
        return visitStructInit(node, parent);
    case NodeTag_error_union:
        return visitErrorUnion(node, parent);
    case NodeTag_error_value:
        return visitErrorValue(node, parent);
    case NodeTag_call:
    case NodeTag_call_comma:
    case NodeTag_call_one:
    case NodeTag_call_one_comma:
    case NodeTag_async_call:
    case NodeTag_async_call_comma:
    case NodeTag_async_call_one:
    case NodeTag_async_call_one_comma:
        return visitCall(node, parent);
    case NodeTag_builtin_call:
    case NodeTag_builtin_call_comma:
    case NodeTag_builtin_call_two:
    case NodeTag_builtin_call_two_comma:
        return visitBuiltinCall(node, parent);
    case NodeTag_address_of:
        return visitAddressOf(node, parent);
    case NodeTag_deref:
        return visitDeref(node, parent);
    case NodeTag_unwrap_optional:
        return visitUnwrapOptional(node, parent);
    case NodeTag_equal_equal:
    case NodeTag_bang_equal:
    case NodeTag_less_or_equal:
    case NodeTag_less_than:
    case NodeTag_greater_than:
    case NodeTag_greater_or_equal:
        return visitCmpExpr(node, parent);
    case NodeTag_bool_and:
    case NodeTag_bool_or:
        return visitBoolExpr(node, parent);
    case NodeTag_bool_not:
        return visitBoolNot(node, parent);
    case NodeTag_mul:
    case NodeTag_div:
    case NodeTag_mod:
    case NodeTag_mul_wrap:
    case NodeTag_mul_sat:
    case NodeTag_add:
    case NodeTag_add_wrap:
    case NodeTag_add_sat:
    case NodeTag_sub:
    case NodeTag_sub_wrap:
    case NodeTag_sub_sat:
    case NodeTag_shl:
    case NodeTag_shl_sat:
    case NodeTag_shr:
    case NodeTag_bit_and:
    case NodeTag_bit_xor:
    case NodeTag_bit_or:
        return visitMathExpr(node, parent);
    case NodeTag_negation:
    case NodeTag_negation_wrap:
        return visitNegation(node, parent);
    case NodeTag_bit_not:
        return visitBitNot(node, parent);
    case NodeTag_try:
        return visitTry(node, parent);
    case NodeTag_catch:
        return visitCatch(node, parent);
    case NodeTag_orelse:
        return visitOrelse(node, parent);
    case NodeTag_array_type:
        return visitArrayType(node, parent);
    case NodeTag_array_init:
    case NodeTag_array_init_comma:
    case NodeTag_array_init_one:
    case NodeTag_array_init_one_comma:
    // TODO: aray_init_dot
        return visitArrayInit(node, parent);
    case NodeTag_array_access:
        return visitArrayAccess(node, parent);
    case NodeTag_array_cat:
        return visitArrayCat(node, parent);
    case NodeTag_for_range:
        return visitForRange(node, parent);
    case NodeTag_slice:
    case NodeTag_slice_open:
    case NodeTag_slice_sentinel:
        return visitSlice(node, parent);
    case NodeTag_array_type_sentinel:
        return visitArrayTypeSentinel(node, parent);
    case NodeTag_if:
    //case NodeTag_if_simple: // not expr?
        return visitIf(node, parent);
    case NodeTag_return:
        return visitReturn(node, parent);
    case NodeTag_break:
        return visitBreak(node, parent);
    case NodeTag_switch:
    case NodeTag_switch_comma:
        return visitSwitch(node, parent);
    case NodeTag_fn_proto:
    case NodeTag_fn_proto_simple:
    case NodeTag_fn_proto_one:
    case NodeTag_fn_proto_multi:
        return visitFnProto(node, parent);
    //case NodeTag_switch_case:
    //case NodeTag_switch_case_inline:
    case NodeTag_block:
    case NodeTag_block_semicolon:
    case NodeTag_block_two:
    case NodeTag_block_two_semicolon:
        return visitBlock(node, parent);

    case NodeTag_grouped_expression:
    //case NodeTag_block_two:
    // case NodeTag_block_two_semicolon:
    case NodeTag_await:
    case NodeTag_comptime:
        visitChildren(node, parent);
        return Continue;
    default:
        break;
    }
    // TODO: Thereset

    return Break;
}

VisitResult ExpressionVisitor::visitBlock(const ZigNode &node, const ZigNode &parent)
{
    // Visit children to set return type if any
    visitChildren(node, parent);
    // TODO: Labeled multiple labeled blocks
    QString label = node.blockLabel();
    if (!label.isEmpty()) {
        if (breakType()) {
            encounter(breakType());
        } else {
            encounterUnknown();
        }
    } else if (
        returnType()
        && returnType().dynamicCast<BuiltinType>()
        && returnType().staticCast<BuiltinType>()->isNoreturn()
    ) {
        // If a noreturn function was called in this block
        encounter(returnType());
    } else {
        encounter(BuiltinType::newFromName(QStringLiteral("void")));
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitBreak(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    const NodeData data = node.data();
    const ZigNode rhs = {node.ast, data.rhs};
    // TODO: Check if label matches block ?
    if (!rhs.isRoot()) {
        QString identifier = node.tokenSlice(data.rhs);
        ExpressionVisitor v(this);
        v.setInferredType(v.inferredType());
        v.startVisiting(rhs, node);
        setBreakType(v.lastType());
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitReturn(const ZigNode &node, const ZigNode &parent)
{
    const auto lhs = node.lhsAsNode();
    if (lhs.isRoot()) {
        setReturnType(BuiltinType::newFromName(QStringLiteral("void")));
    } else {
        ExpressionVisitor v(this);
        v.startVisiting(lhs, node);
        setReturnType(v.lastType());
    }
    visitChildren(node, parent);
    return Continue;
}

VisitResult ExpressionVisitor::visitPointerType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    PtrTypeData ptr_info = ast_ptr_type_data(node.ast, node.index);

    ZigNode childType = {node.ast, ptr_info.child_type};
    ExpressionVisitor v(this);
    v.startVisiting(childType, node);

    auto baseType = v.lastType();
    const bool is_const = ptr_info.info.is_const && !(baseType->modifiers() & AbstractType::ConstModifier);

    int sentinel = -1;
    if (ptr_info.sentinel) {
        ZigNode sentinelNode = {node.ast, ptr_info.sentinel};
        bool ok;
        if (sentinelNode.tag() == NodeTag_number_literal) {
            QString value = sentinelNode.mainToken();
            sentinel = value.toInt(&ok, 0);
        }
    }

    int align = -1;
    if (ptr_info.align_node) {
        bool ok;
        ZigNode alignNode = {node.ast, ptr_info.align_node};
        if (alignNode.tag() == NodeTag_number_literal) {
            QString value = alignNode.mainToken();
            align = value.toInt(&ok, 0);
        }
    }

    QString mainToken = node.mainToken();
    QString nextToken = node.tokenSlice(ast_node_main_token(node.ast, node.index) + 1);
    if (mainToken == QLatin1String("[") && nextToken == QLatin1String("*")) {
        // Pointer to unknown size or cptr
        // eg [*]u8 or [*c]u8
        PointerType::Ptr ptrType(new PointerType);

        if (align > 0) {
            ptrType->setAlignOf(align);
        }

        if (ptr_info.info.is_volatile) {
            ptrType->setModifiers(AbstractType::VolatileModifier);
        }

        ptrType->setModifiers(ptrType->modifiers() | ArrayModifier);
        if (is_const) {
            auto clone = baseType->clone();
            clone->setModifiers(clone->modifiers() | AbstractType::ConstModifier);
            baseType = AbstractType::Ptr(clone);
        }
        ptrType->setBaseType(baseType);
        encounter(ptrType);
    } else if (mainToken == QLatin1String("[")) {
        // Slice
        // eg []u8
        // TODO: slice alignment
        if (is_const) {
            auto clone = baseType->clone();
            clone->setModifiers(clone->modifiers() | AbstractType::ConstModifier);
            baseType = AbstractType::Ptr(clone);
        }

        SliceType::Ptr sliceType(new SliceType);
        sliceType->setElementType(baseType);
        if (sentinel >= 0) {
            sliceType->setSentinel(sentinel);
        }
        if (align > 0) {
            sliceType->setAlignOf(align);
        }
        encounter(sliceType);
    } else if (mainToken == QLatin1String("*")) {
        // Single item pointer
        PointerType::Ptr ptrType(new PointerType);

        if (align > 0) {
            ptrType->setAlignOf(align);
        }

        if (ptr_info.info.is_volatile) {
            ptrType->setModifiers(AbstractType::VolatileModifier);
        }

        if (is_const) {
            // eg *const fn() void
            ptrType->setModifiers(ptrType->modifiers() | AbstractType::ConstModifier);
        }
        ptrType->setBaseType(baseType);
        encounter(ptrType);
    } else {
        // Can this happen ?
        PointerType::Ptr ptrType(new PointerType);
        if (is_const) {
            ptrType->setModifiers(ptrType->modifiers() | AbstractType::ConstModifier);
        }
        ptrType->setBaseType(baseType);
        encounter(ptrType);
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitAddressOf(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit address of";
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    PointerType::Ptr ptrType(new PointerType);
    ptrType->setBaseType(v.lastType());
    encounter(ptrType);
    return Continue;
}

VisitResult ExpressionVisitor::visitDeref(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit deref";
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    if (auto ptr = v.lastType().dynamicCast<PointerType>()) {
        encounter(ptr->baseType());
    } else {
        encounterUnknown(); // TODO: Set error?
    }
    return Continue;
}


VisitResult ExpressionVisitor::visitOptionalType(const ZigNode &node, const ZigNode &parent)
{
    // qCDebug(KDEV_ZIG) << "visit optional";
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    OptionalType::Ptr optionalType(new OptionalType);
    optionalType->setBaseType(v.lastType());
    encounter(optionalType);
    return Continue;
}

VisitResult ExpressionVisitor::visitUnwrapOptional(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);

    // Automatically derefs
    auto T = v.lastType();
    if (const auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
    }
    if (const auto optional = T.dynamicCast<OptionalType>()) {
        encounter(optional->baseType());
    } else {
        encounterUnknown(); // TODO: Set error?
    }
    return Continue;

}

VisitResult ExpressionVisitor::visitStringLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString value = node.spellingName(); // This strips the ""

    SliceType::Ptr sliceType(new SliceType);
    sliceType->setSentinel(0);
    sliceType->setDimension(value.size()); // Main token inclues quotes
    sliceType->setElementType(BuiltinType::newFromName(QStringLiteral("u8")));
    sliceType->setModifiers(AbstractType::ConstModifier);
    sliceType->setComptimeKnownValue(value);

    PointerType::Ptr ptrType(new PointerType);
    ptrType->setBaseType(sliceType);
    encounter(ptrType);

    return Continue;
}

VisitResult ExpressionVisitor::visitMultilineStringLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);

    NodeData data = node.data();
    QString value;
    for (TokenIndex i=data.lhs; i <= data.rhs; i++) {
        QString part = node.tokenSlice(i);
        if ( part.startsWith(QLatin1Char('/')) )
            continue; // Skip doc comment if inline
        if (part.size() > 2)
            value += part.mid(2); // Remove "\\"
    }

    SliceType::Ptr sliceType(new SliceType);
    sliceType->setSentinel(0);
    sliceType->setDimension(value.size());
    sliceType->setElementType(BuiltinType::newFromName(QStringLiteral("u8")));
    sliceType->setModifiers(AbstractType::ConstModifier);
    sliceType->setComptimeKnownValue(value);

    PointerType::Ptr ptrType(new PointerType);
    ptrType->setBaseType(sliceType);
    encounter(ptrType);
    return Continue;
}

VisitResult ExpressionVisitor::visitNumberLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString tok = node.mainToken();
    // qCDebug(KDEV_ZIG) << "visit number lit" << name;
    BuiltinType::Ptr t(new BuiltinType(tok.contains(QStringLiteral(".")) ? QStringLiteral("comptime_float") : QStringLiteral("comptime_int")));
    t->setComptimeKnownValue(tok);
    encounter(t);
    return Continue;
}

VisitResult ExpressionVisitor::visitCharLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString c = node.mainToken();
    if (c.size() > 2) {
        // Remove the '' but keep escapes eg '\n'
        // qCDebug(KDEV_ZIG) << "visit char lit" << c << "size" << c.size();
        BuiltinType::Ptr t(new BuiltinType(QStringLiteral("u8")));
        t->setComptimeKnownValue(c.mid(1, c.size()-2));
        encounter(t);
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitEnumLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.mainToken();
    // qCDebug(KDEV_ZIG) << "visit enum lit" << name;
    if (auto enumType = inferredType().dynamicCast<EnumType>()) {
        if (auto decl = Helper::accessAttribute(enumType, name, topContext())) {
            encounterLvalue(DeclarationPointer(decl));
            return Continue;
        }
    }
    // qCDebug(KDEV_ZIG) << "visit enum unknown";
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitIdentifier(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.mainToken();
    // qCDebug(KDEV_ZIG) << "visit ident" << name;
    if (auto builtinType = BuiltinType::newFromName(name)) {
        encounter(builtinType);
    }
    else if (
        auto* decl = Helper::declarationForName(
            name,
            CursorInRevision::invalid(),
            DUChainPointer<const DUContext>(context()),
            m_excludedDeclaration
        ))
    {
        // DUChainReadLocker lock; // For debug statement only
        // qCDebug(KDEV_ZIG) << "result" << decl->toString();;
        // qCDebug(KDEV_ZIG) << "ident" << name << "found";
        encounterLvalue(DeclarationPointer(decl));
    }
    else {
        // qCDebug(KDEV_ZIG) << "ident" << name << "unknown";
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitMergeErrorSets(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};

    ExpressionVisitor v1(this);
    v1.startVisiting(lhs, node);
    ExpressionVisitor v2(this);
    v2.startVisiting(rhs, node);
    if (auto a = v1.lastType().dynamicCast<EnumType>()) {
        if (auto b = v2.lastType().dynamicCast<EnumType>()) {
            // TODO: Create new error set decl?
            if (a->enumType()) {
                encounter(a->enumType());
            } else if (b->enumType()) {
                encounter(b->enumType());
            } else {
                encounter(a);
            }
        }
    }
    encounterUnknown();
    return Continue;
}


VisitResult ExpressionVisitor::visitFieldAccess(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    NodeData data = node.data();
    ZigNode owner = {node.ast, data.lhs};
    v.startVisiting(owner, node);
    QString attr = node.tokenSlice(data.rhs);
    const auto T = v.lastType();
    // {
    //     DUChainReadLocker lock; // Needed if printing debug statement
    //     const auto isModule = T->modifiers() & ModuleModifier;
    //     qCDebug(KDEV_ZIG) << "Access " << attr << " on" << (isModule ? "module" : "") << v.lastType()->toString() ;
    // }

    if (auto s = T.dynamicCast<SliceType>()) {
        // Slices have builtin len / ptr types
        if (attr == QLatin1String("len")) {
            encounter(BuiltinType::newFromName(QStringLiteral("usize")));
        } else if (attr == QLatin1String("ptr")) {
            PointerType::Ptr ptr(new PointerType);
            ptr->setModifiers(s->modifiers());
            ptr->setBaseType(s->elementType());
            encounter(ptr);
        } else {
            encounterUnknown();
        }
        return Continue;
    }

    if (auto *decl = Helper::accessAttribute(T, attr, topContext())) {
        // DUChainReadLocker lock; // Needed if printing debug statement
        //qCDebug(KDEV_ZIG) << " result " << decl->toString() << "from" << decl->url();
        encounterLvalue(DeclarationPointer(decl));
    }
    else {
        // qCDebug(KDEV_ZIG) << " no result ";
        encounterUnknown();
    }
    return Continue;
}

// Should not be used for any dot inits
VisitResult ExpressionVisitor::visitStructInit(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    // Union is subclass of StructureType so this branch must come first
    if (auto u = v.lastType().dynamicCast<UnionType>()) {
        NodeTag tag = node.tag();
        if (tag == NodeTag_struct_init_one || tag == NodeTag_struct_init_one_comma) {
            FieldInitData fieldData = node.structInitAt(0);
            QString fieldName = node.tokenSlice(fieldData.name_token);
            if (auto decl = Helper::accessAttribute(u, fieldName, topContext())) {
                ZigNode valueNode = {node.ast, fieldData.value_expr};
                ExpressionVisitor f(this);
                f.startVisiting(valueNode, node);
                auto v = dynamic_cast<ComptimeType*>(f.lastType().data());
                if (v && v->isComptimeKnown()) {
                    UnionType::Ptr unionValue(static_cast<UnionType*>(decl->abstractType()->clone()));
                    unionValue->setComptimeKnownValue(v->comptimeKnownValue());
                    encounter(unionValue, DeclarationPointer(decl));
                } else {
                    encounterLvalue(DeclarationPointer(decl));
                }
                return Continue;
            }
        }
    } else if (auto s = v.lastType().dynamicCast<StructureType>()) {
        encounter(s);
        return Continue;
    }
    encounterUnknown();
    return Continue;
}


VisitResult ExpressionVisitor::visitContainerDecl(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    NodeKind kind = parent.kind();
    if (kind == VarDecl) {
        QString name = parent.spellingName();
        Declaration* decl = Helper::declarationForName(
            name,
            CursorInRevision::invalid(),
            DUChainPointer<const DUContext>(context())
        );
        if (decl) {
            encounterLvalue(DeclarationPointer(decl));
            return Continue;
        }
    }


    Identifier ident(node.containerName());
    DUChainReadLocker lock;
    {
        auto decls = context()->findLocalDeclarations(ident);
        if (!decls.isEmpty()) {
            encounterLvalue(DeclarationPointer(decls.first()));
            return Continue;
        }
    }


    for (auto childContext: context()->childContexts()) {
        auto decls = childContext->findLocalDeclarations(ident);
        if (!decls.isEmpty()) {
            encounterLvalue(DeclarationPointer(decls.first()));
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitBuiltinCall(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.mainToken();
    if (name == QLatin1String("@as")) {
        return callBuiltinAs(node);
    } else if (name == QLatin1String("@This")) {
        return callBuiltinThis(node);
    } else if (name == QLatin1String("@import")) {
        return callBuiltinImport(node);
    } else if (name == QLatin1String("@cImport")) {
        return callBuiltinCImport(node);
    } else if (name == QLatin1String("@cInclude")) {
        return callBuiltinCInclude(node);
    } else if (name == QLatin1String("@typeInfo")) {
        return callBuiltinTypeInfo(node);
    } else if (name == QLatin1String("@TypeOf")) {
        return callBuiltinTypeOf(node);
    } else if (name == QLatin1String("@fieldParentPtr")) {
        return callBuiltinFieldParentPtr(node);
    } else if (name == QLatin1String("@field")) {
        return callBuiltinField(node);
    } else if (name == QLatin1String("@intFromFloat")) {
        return callBuiltinIntFromFloat(node);
    } else if (name == QLatin1String("@floatFromInt")) {
        return callBuiltinFloatFromInt(node);
    } else if (name == QLatin1String("@intFromBool")) {
        return callBuiltinIntFromBool(node);
    } else if (name == QLatin1String("@boolFromInt")) {
        return callBuiltinBoolFromInt(node);
    } else if (name == QLatin1String("@intCast")) {
        return callBuiltinIntCast(node);
    } else if (name == QLatin1String("@enumFromInt")) {
        return callBuiltinEnumFromInt(node);
    } else if (name == QLatin1String("@intFromEnum")) {
        return callBuiltinIntFromEnum(node);
    } else if (name == QLatin1String("@Vector")) {
        return callBuiltinVector(node);
    } else if (name == QLatin1String("@reduce")) {
        return callBuiltinReduce(node);
    } else if (name == QLatin1String("@splat")) {
        return callBuiltinSplat(node);
    // todo @Type
    // todo @Type
    } else if (
        // These return the type of the first argument
        name == QLatin1String("@sqrt")
        || name == QLatin1String("@sin")
        || name == QLatin1String("@cos")
        || name == QLatin1String("@tan")
        || name == QLatin1String("@exp")
        || name == QLatin1String("@exp2")
        || name == QLatin1String("@log")
        || name == QLatin1String("@log2")
        || name == QLatin1String("@log10")
        || name == QLatin1String("@floor")
        || name == QLatin1String("@ceil")
        || name == QLatin1String("@trunc")
        || name == QLatin1String("@round")
        || name == QLatin1String("@min")
        || name == QLatin1String("@max")
        || name == QLatin1String("@mod")
        || name == QLatin1String("@rem")
        || name == QLatin1String("@abs")
        || name == QLatin1String("@shlExact")
        || name == QLatin1String("@shrExact")
        || name == QLatin1String("@mulAdd")
        || name == QLatin1String("@atomicLoad")
    ) {
        ExpressionVisitor v(this);
        v.startVisiting(node.nextChild(), node);
        encounter(v.lastType());
    } else if (
        name == QLatin1String("@errorName")
        || name == QLatin1String("@tagName")
        || name == QLatin1String("@typeName")
        || name == QLatin1String("@embedFile")
    ) {
        auto slice = new SliceType();
        slice->setSentinel(0);
        // Must clone since constness is modified here
        auto elemType = BuiltinType::newFromName(QStringLiteral("u8"))->clone();
        elemType->setModifiers(AbstractType::ConstModifier);
        slice->setElementType(AbstractType::Ptr(elemType));
        encounter(AbstractType::Ptr(slice));
    } else if (
        name == QLatin1String("@intFromPtr")
        || name == QLatin1String("@returnAddress")
    ) {
        encounter(BuiltinType::newFromName(QStringLiteral("usize")));
    } else if (
        name == QLatin1String("@memcpy")
        || name == QLatin1String("@memset")
        || name == QLatin1String("@setCold")
        || name == QLatin1String("@setAlignStack")
        || name == QLatin1String("@setEvalBranchQuota")
        || name == QLatin1String("@setFloatMode")
        || name == QLatin1String("@setRuntimeSafety")
    ) {
        encounter(BuiltinType::newFromName(QStringLiteral("void")));
    } else if (
        name == QLatin1String("@alignOf")
        || name == QLatin1String("@sizeOf")
        || name == QLatin1String("@bitOffsetOf")
        || name == QLatin1String("@bitSizeOf")
        || name == QLatin1String("@offsetOf")
    ) {
        encounter(BuiltinType::newFromName(QStringLiteral("comptime_int")));
    } else if (
        name == QLatin1String("@hasField")
        || name == QLatin1String("@hasDecl")
    ) {
        encounter(BuiltinType::newFromName(QStringLiteral("bool")));
    } else if (name == QLatin1String("@trap")) {
        encounter(BuiltinType::newFromName(QStringLiteral("noreturn")));
    } else if (
        name == QLatin1String("@panic")
        || name == QLatin1String("@compileError")
        || name == QLatin1String("@compileLog")
    ) {
        encounter(BuiltinType::newFromName(QStringLiteral("trap")));
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinIntFromFloat(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isInteger() && node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        const auto value = v.lastType().dynamicCast<BuiltinType>();
        if (value && value->isFloat()) {
            // TODO: handle comptime known
            encounter(result);
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinFloatFromInt(const ZigNode &node)
{
    // qCDebug(KDEV_ZIG) << "callBuiltinFloatFromInt";
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isFloat() && node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        const auto value = v.lastType().dynamicCast<BuiltinType>();
        if (value && value->isInteger()) {
            // TODO: handle comptime known
            encounter(result);
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinIntFromBool(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isInteger() && node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        const auto value = v.lastType().dynamicCast<BuiltinType>();
        if (value && value->isBool()) {
            if (value->isTrue() || value->isFalse()) {
                BuiltinType::Ptr r(static_cast<BuiltinType*>(result->clone()));
                r->setComptimeKnownValue(value->isTrue() ? QStringLiteral("1") : QStringLiteral("0"));
                encounter(r);
            } else {
                encounter(result);
            }
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinBoolFromInt(const ZigNode &node)
{
    if (node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        const auto value = v.lastType().dynamicCast<BuiltinType>();
        if (value && value->isInteger()) {
            if (value->isComptimeKnown()) {
                bool ok;
                bool val = false;
                QString s = value->comptimeKnownValue().str();
                if (value->isSigned()) {
                    val = s.toLongLong(&ok, 0);
                } else {
                    val = s.toULongLong(&ok, 0);
                }
                if (ok) {
                    encounter(BuiltinType::newFromName( val ? QStringLiteral("true") : QStringLiteral("false")));
                    return Continue;
                }
            }
        }
        encounter(BuiltinType::newFromName(QStringLiteral("bool")));
        return Continue;
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinIntCast(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isInteger() && node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        const auto value = v.lastType().dynamicCast<BuiltinType>();
        if (value && value->isInteger()) {
            if (value->isComptimeKnown()) {
                // TODO: implement cast
                encounter(result);
            } else {

                encounter(result);
            }
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinEnumFromInt(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<EnumType>();
    if (result && node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        const auto value = v.lastType().dynamicCast<BuiltinType>();
        if (value && value->isInteger()) {
            // TODO: handle comptime known
            if (result->enumType()) {
                encounter(result->enumType());
            } else {
                encounter(result);
            }
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinIntFromEnum(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isInteger() && node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        if (const auto value = v.lastType().dynamicCast<EnumType>()) {
            // TODO: handle comptime known
            encounter(result);
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinThis(const ZigNode &node)
{
    // TODO: Report problem if arguments ?
    auto range = node.range();
    DUChainReadLocker lock;
    if (auto thisCtx = Helper::thisContext(range.start, topContext())) {
        if (auto owner = thisCtx->owner()) {
            encounterLvalue(DeclarationPointer(owner));
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinTypeOf(const ZigNode &node)
{
    if (node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        encounter(v.lastType()); // TODO: Flag for instance/type ?
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinTypeInfo(const ZigNode &node)
{
    auto decl = Helper::declarationForImportedModuleName(
        QStringLiteral("std.builtin.Type"), session()->document().str());
    if (decl) {
        auto Type = decl->abstractType().dynamicCast<UnionType>();
        if (node.isBuiltinCallTwo() && Type) {
            ExpressionVisitor v(this);
            v.startVisiting(node.lhsAsNode(), node);
            QString typeName;
            if (auto value = v.lastType().dynamicCast<BuiltinType>()) {
                if (value->isType())
                    typeName = QStringLiteral("Type");
                else if (value->isVoid())
                    typeName = QStringLiteral("Void");
                else if (value->isBool())
                    typeName = QStringLiteral("Bool");
                else if (value->isComptimeInt())
                    typeName = QStringLiteral("ComptimeInt");
                else if (value->isComptimeFloat())
                    typeName = QStringLiteral("ComptimeFloat");
                else if (value->isInteger())
                    typeName = QStringLiteral("Int");
                else if (value->isFloat())
                    typeName = QStringLiteral("Float");
                else if (value->isUndefined())
                    typeName = QStringLiteral("Undefined");
                else if (value->isNull())
                    typeName = QStringLiteral("Null");
                else if (value->isFrame())
                    typeName = QStringLiteral("Frame");
                else if (value->isAnyframe())
                    typeName = QStringLiteral("AnyFrame");
                else if (value->isNoreturn())
                    typeName = QStringLiteral("NoReturn");
            } else if (v.lastType().dynamicCast<FunctionType>()) {
                typeName = QStringLiteral("Fn");
            } else if (v.lastType().dynamicCast<UnionType>()) {
                typeName = QStringLiteral("Union"); // Must come before struct
            } else if (v.lastType().dynamicCast<StructureType>()) {
                typeName = QStringLiteral("Struct");
            } else if (v.lastType().dynamicCast<ErrorType>()) {
                typeName = QStringLiteral("ErrorUnion"); // TODO: Is this right ?
            } else if (v.lastType().dynamicCast<PointerType>()) {
                typeName = QStringLiteral("Pointer");
            } else if (v.lastType().dynamicCast<OptionalType>()) {
                typeName = QStringLiteral("Optional");
            } else if (auto enumType = v.lastType().dynamicCast<EnumType>()) {
                // TOOD: Error set ?
                if (enumType->enumType().dynamicCast<EnumType>())
                    typeName = QStringLiteral("EnumLiteral");
                else
                    typeName = QStringLiteral("Enum");
            } else if (auto slice = v.lastType().dynamicCast<SliceType>()) {
                if (slice->dimension() > 0)
                    typeName = QStringLiteral("Array");
                else
                    typeName = QStringLiteral("Slice");
            }

            if (auto T = Helper::accessAttribute(Type, typeName, topContext())) {
                encounterLvalue(DeclarationPointer(T));
                return Continue;
            }
        }
        encounterLvalue(DeclarationPointer(decl));
        return Continue;
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinImport(const ZigNode &node)
{
    // TODO: Report problem if invalid argument
    ZigNode strNode = node.lhsAsNode();
    if (strNode.tag() != NodeTag_string_literal) {
        encounterUnknown();
        return Continue;
    }
    QString importName = strNode.spellingName();

    QUrl importPath = Helper::importPath(importName, session()->document().str());
    if (importPath.isEmpty()) {
        encounterUnknown();
        return Continue;
    }

    DUChainReadLocker lock;
    auto *importedModule = DUChain::self()->chainForDocument(importPath);
    if (importedModule && importedModule->owner()) {
        auto mod = importedModule->owner();
        if (mod && mod->abstractType().data()) {
            Q_ASSERT(mod->abstractType()->modifiers() & ModuleModifier);
            // qCDebug(KDEV_ZIG) << "Imported module " << mod->toString() << "from" << importPath.toString() << "ctx" << importedModule;
            encounterLvalue(DeclarationPointer(mod));
            return Continue;
        }
        // TODO: Reschedule ?
        qCDebug(KDEV_ZIG) << "Module has no declarations" << importPath.toString();
    } else {
        IndexedString dependency(importPath);
        Helper::scheduleDependency(dependency, session()->jobPriority());
        m_session->addUnresolvedImport(dependency);
        // TODO: This does not work...
        // auto dep = new ScheduleDependency(
        //     const_cast<KDevelop::ParseJob*>(session()->job()),
        //     session()->document(),
        //     dependency,
        //     session()->jobPriority());
        DelayedType::Ptr delayedImport(new DelayedType);
        delayedImport->setModifiers(ModuleModifier);
        delayedImport->setIdentifier(dependency);
        encounter(delayedImport);
        return Continue;
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinFieldParentPtr(const ZigNode &node)
{
    // TODO: This assumes correct arguments
    // TODO: Handle different versions
    Q_UNUSED(node);
    // if (zigVersion < 0.12.0) {
    //     ZigNode typeNode = node.nextChild();
    //     ExpressionVisitor v(this);
    //     v.startVisiting(typeNode, node);
    //     PointerType::Ptr ptr(new PointerType);
    //     ptr->setBaseType(v.lastType());
    // else {
    if (auto ptr = inferredType().dynamicCast<PointerType>()) {
        // TODO: Validate second arg field
        encounter(ptr);
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinField(const ZigNode &node)
{
    auto tag = node.tag();
    if (tag == NodeTag_builtin_call_two || tag == NodeTag_builtin_call_two_comma) {
        NodeData data = node.data();
        ZigNode lhs = {node.ast, data.lhs};
        ExpressionVisitor v(this);
        v.startVisiting(lhs, node);

        ZigNode rhs = {node.ast, data.rhs};
        QString fieldName;
        if (rhs.tag() == NodeTag_string_literal) {
            fieldName = rhs.spellingName();
        } else {
            // May be some expr like @import("foo").bar or some const var
            ExpressionVisitor nameVisitor(this);
            nameVisitor.startVisiting(rhs, node);
            // Look for *const [x:0]u8
            if (auto ptr = nameVisitor.lastType().dynamicCast<PointerType>()) {
                if (auto slice = ptr->baseType().dynamicCast<SliceType>()) {
                    if (slice->isComptimeKnown()) {
                        fieldName = slice->comptimeKnownValue().str();
                    }
                }
            }
        }

        if (fieldName.isEmpty()) {
            encounterUnknown();
            return Continue;;
        }

        // TODO: lhs may be a type or instance
        if (auto r = Helper::accessAttribute(v.lastType(), fieldName, topContext())) {
            encounterLvalue(DeclarationPointer(r));
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinAs(const ZigNode&node)
{
    NodeTag tag = node.tag();
    if (tag == NodeTag_builtin_call_two || tag == NodeTag_builtin_call_two_comma) {
        NodeData data = node.data();
        ZigNode lhs = {node.ast, data.lhs};
        ZigNode rhs = {node.ast, data.rhs};

        ExpressionVisitor typeVisitor(this);
        typeVisitor.startVisiting(lhs, node);

        // If lhs is a builtin type, check if rhs is comptime known and if
        // so set it to a comptime known value, eg @as(u32, 1)
        if (auto builtin = typeVisitor.lastType().dynamicCast<BuiltinType>()) {
            ExpressionVisitor valueVisitor(this);
            valueVisitor.setInferredType(typeVisitor.lastType());
            valueVisitor.startVisiting(rhs, node);
            if (auto value = dynamic_cast<ComptimeType*>(valueVisitor.lastType().data())) {
                if (value->isComptimeKnown() && builtin->canValueBeAssigned(value->asType())) {
                    BuiltinType::Ptr t(static_cast<BuiltinType*>(builtin->clone()));
                    t->setComptimeKnownValue(value->comptimeKnownValue());
                    encounter(t);
                    return Continue;
                }
            }
        }
        encounter(typeVisitor.lastType());
        return Continue;
    }
    encounterUnknown();
    return Continue;
}


VisitResult ExpressionVisitor::callBuiltinVector(const ZigNode &node)
{
    if (node.isBuiltinCallTwo()) {
        NodeData data = node.data();
        ZigNode lhs = {node.ast, data.lhs};
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor typeVisitor(this);
        typeVisitor.startVisiting(rhs, node);
        VectorType::Ptr vectorType(new VectorType);
        // TODO: Check type vs instance?
        vectorType->setElementType(typeVisitor.lastType());

        if (lhs.tag() == NodeTag_number_literal) {
            bool ok;
            int size = lhs.mainToken().toInt(&ok, 0);
            if (ok) {
                vectorType->setDimension(size);
            }
        }
        // TODO: Else case ?
        encounter(vectorType);
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinReduce(const ZigNode &node)
{
    if (node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.rhsAsNode(), node);
        if (auto vector = v.lastType().dynamicCast<VectorType>()) {
            encounter(vector->elementType());
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinSplat(const ZigNode &node)
{
    if (node.isBuiltinCallTwo()) {
        ExpressionVisitor v(this);
        v.startVisiting(node.lhsAsNode(), node);
        VectorType::Ptr vectorType(new VectorType);
        // TODO: Validate type is int, bool, float, or ptr
        vectorType->setElementType(v.lastType());
        if (auto r = inferredType().dynamicCast<VectorType>()) {
            vectorType->setDimension(r->dimension());
            if ( Helper::canTypeBeAssigned(r->elementType(), v.lastType()) ) {
               vectorType->setElementType(r->elementType());
            }
        }
        else if (auto r = inferredType().dynamicCast<SliceType>()) {
            vectorType->setDimension(r->dimension());
            if ( Helper::canTypeBeAssigned(r->elementType(), v.lastType()) ) {
               vectorType->setElementType(r->elementType());
            }
        }
        encounter(vectorType);
        return Continue;
    }
    encounterUnknown();
    return Continue;
}


VisitResult ExpressionVisitor::callBuiltinCImport(const ZigNode &node)
{
    // This assumes the original ExpressionVisitor callee
    // set the inferredType to the correct struct type declaration for the
    // cInclude visitor to add context to or it will not work
    if (node.isBuiltinCallTwo()
        && inferredType().dynamicCast<StructureType>()
        && inferredType()->modifiers() & ModuleModifier
    ) {
        // Cannot create a new type here because the context cannot be modified
        // (or idk how)
        // StructureType::Ptr cImportStruct(new StructureType);
        // cImportStruct->setModifiers(ModuleModifier | CIncludeModifier);
        ExpressionVisitor v(this);
        v.setInferredType(inferredType());
        // This should visit the @cInclude statements
        // that modify the context
        v.startVisiting(node.lhsAsNode(), node);
        encounter(inferredType());
        return Continue;
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinCInclude(const ZigNode &node)
{
    auto cImportStruct = inferredType().dynamicCast<StructureType>();
    if (node.isBuiltinCallTwo() && cImportStruct && cImportStruct->modifiers() & ModuleModifier) {
        ZigNode strNode = node.lhsAsNode();
        if (strNode.tag() != NodeTag_string_literal) {
            encounterUnknown();
            return Continue;
        }
        QString header = strNode.spellingName();
        if (header.isEmpty()) {
            encounterUnknown();
            return Continue;
        }
        // qCDebug(KDEV_ZIG) << "cInclude " << header;
        QUrl includePath = Helper::includePath(header, session()->document().str());
        // TODO: find real include path?
        IndexedString dependency(includePath);
        DUChainWriteLocker lock;
        auto *includedModule = DUChain::self()->chainForDocument(dependency);
        if (includedModule) {
            if (auto ctx = cImportStruct->internalContext(topContext())) {
                // qCDebug(KDEV_ZIG) << "cInclude(" << includePath.path() << ") added to cImport";
                ctx->addImportedParentContext(includedModule);
            } else {
                qCDebug(KDEV_ZIG) << "cInclude(" << includePath.path() << ") cImport context is null";
            }
        } else {
            qCDebug(KDEV_ZIG) << "cInclude(" << includePath.path() << ") null, scheduling";
            Helper::scheduleDependency(dependency, session()->jobPriority());
            m_session->addUnresolvedImport(dependency);
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitCall(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // QString functionName = node.spellingName();
    // if (functionName == "@compileError") {
    //     encounter(new BuiltinType("compileError"));
    // }
    ExpressionVisitor v(this);
    ZigNode next = node.nextChild();
    v.startVisiting(next, node);
    auto func = v.lastType().dynamicCast<FunctionType>();
    // Return type may temporarily be null when the FunctionVisitor is
    // visiting a recursive function that returns "type"
    if (!func || !func->returnType()) {
        encounterUnknown();
        return Continue;
    }

    auto returnType = func->returnType();
    // Shortcut, builtintypes cant have delayed types
    if (auto builtin = returnType.dynamicCast<BuiltinType>()) {
        if (builtin->isNoreturn()) {
            setReturnType(builtin);
        }
        encounter(returnType);
        return Continue;
    }

    // Check if any delayed types need resolved
    // It should be able to resolve the type name in the proper context
    // using kdevelops builtin specialization but I'm still not sure
    // how to properly do that, so until then this is a workaround
    const auto &args = func->arguments();
    int startArg = 0;
    AbstractType::Ptr selfType  = v.functionCallSelfType(next, node);
    const auto n = node.callParamCount();
    if (args.size() == n + 1) {
        startArg = 1; // Extra arg should mean an implied "self"
    }

    // eg fn (comptime T: type, ...) []T
    DelayedTypeFinder finder;
    returnType->accept(&finder);
    if (finder.delayedTypes.size() > 0) {
        qCDebug(KDEV_ZIG) << "visit delayed return type" << startArg;
        uint32_t i = 0;
        // Resolve types
        QMap<IndexedString, AbstractType::Ptr> resolvedTypes;

        // If we have a method with a DelayedType or *DelayedType
        // we know it is the same as "self"
        if (startArg == 1 && selfType.data()) {
            if (auto t = args.at(0).dynamicCast<Zig::PointerType>()) {
                if (auto param = t->baseType().dynamicCast<Zig::DelayedType>()) {
                    resolvedTypes.insert(param->identifier(), selfType);
                }
            } else if (auto param = args.at(0).dynamicCast<Zig::DelayedType>()) {
                resolvedTypes.insert(param->identifier(), selfType);
            }
        }

        for (const auto &arg: args.mid(startArg)) {
            if (auto param = arg.dynamicCast<Zig::DelayedType>()) {
                ZigNode argValueNode = node.callParamAt(i);
                if (!argValueNode.isRoot()) {
                    ExpressionVisitor valueVisitor(this);
                    valueVisitor.startVisiting(argValueNode, node);
                    if (!resolvedTypes.contains(param->identifier())) {
                        resolvedTypes.insert(param->identifier(), valueVisitor.lastType());
                    }
                }
            }
            i += 1;
        }

        // Replace resolved
        for (const auto &t: finder.delayedTypes) {
            auto value = resolvedTypes.constFind(t->identifier());
            if (value != resolvedTypes.constEnd()) {
                SimpleTypeExchanger exchanger(t, *value);
                returnType = exchanger.exchange(AbstractType::Ptr(returnType->clone()));
            }
        }
    }
    encounter(returnType);
    return Continue;
}


VisitResult ExpressionVisitor::visitErrorUnion(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit pointer";
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor errorVisitor(this);
    errorVisitor.startVisiting(lhs, node);
    ExpressionVisitor typeVisitor(this);
    typeVisitor.startVisiting(rhs, node);
    ErrorType::Ptr errType(new ErrorType);
    errType->setBaseType(typeVisitor.lastType());
    errType->setErrorType(errorVisitor.lastType());
    encounter(errType);
    return Continue;
}

VisitResult ExpressionVisitor::visitErrorValue(const ZigNode &node, const ZigNode &parent)
{
    // TODO: get actual value
    Q_UNUSED(node);
    Q_UNUSED(parent);
    encounter(BuiltinType::newFromName(QStringLiteral("anyerror")));
    return Continue;
}

VisitResult ExpressionVisitor::visitCmpExpr(const ZigNode &node, const ZigNode &parent)
{
    // TODO: eval exprs..
    Q_UNUSED(node);
    Q_UNUSED(parent);
    encounter(BuiltinType::newFromName(QStringLiteral("bool")));
    return Continue;
}

VisitResult ExpressionVisitor::visitBoolExpr(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeTag tag = node.tag();
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ExpressionVisitor v1(this);
    v1.startVisiting(lhs, node);
    if (const auto a = v1.lastType().dynamicCast<BuiltinType>()) {
        if (!a->isBool()) {
            encounterUnknown();
            return Continue;
        }

        // If lhs is comptime known try to eval it without doing the rhs
        if ((tag == NodeTag_bool_or && a->isTrue()) || (tag == NodeTag_bool_and && a->isFalse())) {
            encounter(a);
            return Continue;
        }

        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(this);
        v2.startVisiting(rhs, node);
        if (const auto b = v2.lastType().dynamicCast<BuiltinType>()) {
            if (!b->isBool()) {
                encounterUnknown();
                return Continue;
            }

            // Can still eval if other side is not comptime known
            if ((tag == NodeTag_bool_or && b->isTrue()) || (tag == NodeTag_bool_and && b->isFalse())) {
                encounter(b);
                return Continue;
            }

            // If both are known do full eval
            if (a->isComptimeKnown() && b->isComptimeKnown()) {
                if (tag == NodeTag_bool_or) {
                    if (a->isTrue() || b->isTrue()) {
                        encounter(BuiltinType::newFromName(QStringLiteral("true")));
                    } else {
                        encounter(BuiltinType::newFromName(QStringLiteral("false")));
                    }
                } else {
                    if (a->isTrue() && b->isTrue()) {
                        encounter(BuiltinType::newFromName(QStringLiteral("true")));
                    } else {
                        encounter(BuiltinType::newFromName(QStringLiteral("false")));
                    }
                }
                return Continue;
            }

            // Both types are only runtime known
            encounter(BuiltinType::newFromName(QStringLiteral("bool")));
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitBoolNot(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // TODO: Evaluate if both lhs & rhs are comptime known
    ZigNode lhs = {node.ast, node.data().lhs};
    ExpressionVisitor v(this);
    v.startVisiting(lhs, node);
    if (const auto a = v.lastType().dynamicCast<BuiltinType>()) {
        if (a->isBool()) {
            if (a->isTrue()) {
                encounter(BuiltinType::newFromName(QStringLiteral("false")));
            } else if (a->isFalse()) {
                encounter(BuiltinType::newFromName(QStringLiteral("true")));
            } else {
                encounter(a); // Not comptime known
            }
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitMathExpr(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};

    ExpressionVisitor v1(this);
    v1.startVisiting(lhs, node);
    // TODO: What does zig do? Try to infer from other side?
    // if (Helper::isMixedType(T)) {
    //     ExpressionVisitor v2(this);
    //     v2.startVisiting(rhs, node);
    //
    //     ExpressionVisitor v3(this);
    //     v3.setInferredType(v2.lastType());
    //     v3.startVisiting(lhs, node);
    //     T = v1.lastType();
    // }

    // Check if lhs & rhs are compatable
    if (const auto a = v1.lastType().dynamicCast<BuiltinType>()) {
        ExpressionVisitor v2(this);
        v2.setInferredType(a); // TODO:
        v2.startVisiting(rhs, node);
        if (const auto b = v2.lastType().dynamicCast<BuiltinType>()) {
            // If both are comptime known try to evaluate the expr
            if (a->isComptimeKnown() && b->isComptimeKnown()) {
                NodeTag tag = node.tag();
                if (a->isUnsigned() && b->isUnsigned()) {
                    encounter(Helper::evaluateUnsignedOp(a, b, tag));
                    return Continue;
                }
                // TODO: The rest
                encounter(a);
                return Continue;
            }
            // if the types match, return the non-comptime one
            else if ((a->isFloat() && b->isFloat())
                || (a->isSigned() && b->isSigned())
                || (a->isUnsigned() && b->isUnsigned())
            ) {
                // TODO: Evaluate comptime known expr
                // TODO: Does not expand widths eg u1 | u8 --> u8
                encounter(a->isComptimeKnown() ? b : a);
                return Continue;
            }
            // else error, must cast
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitNegation(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    if (auto a = v.lastType().dynamicCast<BuiltinType>()) {
        if (a->isSigned() || a->isFloat()) {
            if (a->isComptimeKnown()) {
                // Known to be at least size of 1
                BuiltinType::Ptr result(new BuiltinType(a->dataType()));
                QString value = a->comptimeKnownValue().str();
                if (value.at(0) == QLatin1Char('-')) {
                    result->setComptimeKnownValue(value.mid(1));
                } else {
                    result->setComptimeKnownValue(QStringLiteral("-%1").arg(value));
                }
                encounter(result);
            } else {
                encounter(a);
            }
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitBitNot(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode child = node.nextChild();
    ExpressionVisitor v(this);
    v.startVisiting(child, node);
    if (auto a = v.lastType().dynamicCast<BuiltinType>()) {
        if (a->isInteger()) {
            encounter(a);
            return Continue;
        }
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::visitTry(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    if (auto errorType = v.lastType().dynamicCast<ErrorType>()) {
        encounter(errorType->baseType());
    } else {
        encounterUnknown(); // TODO: Show error?
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitCatch(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ExpressionVisitor v(this);
    v.startVisiting(lhs, node);
    // TODO: Check that lhs and rhs are compatable
    if (auto errorType = v.lastType().dynamicCast<ErrorType>()) {
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(this);
        v2.startVisiting(rhs, node);

        auto baseType = errorType->baseType();

        // Make type optional if it has catch null but not catch return null
        if (auto builtin = v2.exprType().dynamicCast<BuiltinType>()) {
            if (!v2.returnType() && builtin->isNull() && !baseType.dynamicCast<OptionalType>()) {
                OptionalType::Ptr result(new OptionalType());
                result->setBaseType(baseType);
                encounter(result);
                return Continue;
            }
        }
        encounter(baseType);
    } else {
        encounterUnknown(); // TODO: Show error?
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitOrelse(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ExpressionVisitor v1(this);
    v1.startVisiting(lhs, node);
    if (auto optionalType = v1.lastType().dynamicCast<OptionalType>()) {
        // Returns base type
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(this);
        v2.startVisiting(rhs, node);
        encounter(optionalType->baseType());
        return Continue;
    }
    encounterUnknown(); // TODO: Show error?
    return Continue;
}

VisitResult ExpressionVisitor::visitIf(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    IfData if_data = ast_if_data(node.ast, node.index);

    ZigNode condNode = {node.ast, if_data.cond_expr};
    ZigNode thenNode = {node.ast, if_data.then_expr};
    ZigNode elseNode = {node.ast, if_data.else_expr};

    ExpressionVisitor condExpr(this);

    // If capture is present we need to run the expr in proper context
    auto thenContext = context();
    if (if_data.payload_token != INVALID_TOKEN) {
        auto range = node.tokenRange(if_data.payload_token);
        DUChainReadLocker lock;
        thenContext = context()->findContextAt(range.end);
    }
    ExpressionVisitor thenExpr(this, thenContext);
    thenExpr.setInferredType(inferredType());

    auto elseContext = context();
    if (if_data.error_token != INVALID_TOKEN) {
        auto range = node.tokenRange(if_data.error_token);
        DUChainReadLocker lock;
        elseContext = context()->findContextAt(range.end);
    }
    ExpressionVisitor elseExpr(this, elseContext);
    elseExpr.setInferredType(inferredType());

    condExpr.startVisiting(condNode, node);

    // If the condition is comptime known there are 4 cases
    // true, false, null are all comptime known builtins.
    // otherwise if is a comptime known optional we know to only use the then
    if (const auto cond = condExpr.lastType().dynamicCast<BuiltinType>()) {
        if (cond->isTrue()) {
            thenExpr.startVisiting(thenNode, node);
            encounter(thenExpr.lastType());
            return Continue;
        } else if (cond->isFalse() || cond->isNull()) {
            elseExpr.startVisiting(elseNode, node);
            encounter(elseExpr.lastType());
            return Continue;
        }
        // else condition is not a comptime known result
    }
    else if (const auto cond = condExpr.lastType().dynamicCast<OptionalType>()) {
        if (cond->isComptimeKnown()) {
            if (if_data.payload_token == INVALID_TOKEN) {
                encounterUnknown(); // If on optional with no capture is an error
                return Continue;
            }
            thenExpr.startVisiting(thenNode, node);
            encounter(thenExpr.lastType());
            return Continue;
        }
    }
    thenExpr.startVisiting(thenNode, node);
    elseExpr.startVisiting(elseNode, node);
    encounter(Helper::mergeTypes(thenExpr.lastType(), elseExpr.lastType(), context()));
    return Continue;
}

VisitResult ExpressionVisitor::visitSwitch(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);

    const NodeSubRange subrange = node.subRange();
    if (!subrange.isValid()) {
        qCDebug(KDEV_ZIG) << "switch subrange is invalid" << node.index;
        encounterUnknown();
        return Continue;
    }
    if (const auto switchValue = dynamic_cast<ComptimeType*>(v.lastType().data())) {
        if (switchValue->isComptimeKnown()) {
            // {
            //     DUChainReadLocker lock;
            //     qCDebug(KDEV_ZIG) << "switch is comptime known" << switchValue->toString();
            // }
            // Try to return matching case
            for (uint32_t j=subrange.start; j<subrange.end; j++) {
                const ZigNode caseNode = node.extraDataAsNode(j);
                Q_ASSERT(!caseNode.isRoot());
                // qCDebug(KDEV_ZIG) << "checking case" << j << "node tag is" << caseNode.tag();
                const auto n = caseNode.switchCaseCount();
                for (uint32_t i=0; i<n; i++) {
                    const ZigNode caseValue = caseNode.switchCaseItemAt(i);
                    // qCDebug(KDEV_ZIG) << "checking case item" << i << "of case" << j << "tag" << caseValue.tag();
                    ExpressionVisitor caseVisitor(this);
                    caseVisitor.setInferredType(switchValue->asType());
                    caseVisitor.startVisiting(caseValue, caseNode);
                    // TODO: Does not handle ranges?
                    // {
                    //     DUChainReadLocker lock;
                    //     qCDebug(KDEV_ZIG) << "checking switch item" << caseVisitor.lastType()->toString();
                    // }
                    if (switchValue->asType()->equals(caseVisitor.lastType().data())) {
                        ZigNode rhs = {caseNode.ast, caseNode.data().rhs};
                        ExpressionVisitor valueVisitor(this);
                        valueVisitor.setInferredType(inferredType());
                        valueVisitor.startVisiting(rhs, caseNode);
                        encounter(valueVisitor.lastType());
                        return Continue;
                    }
                }
            }
        }
    }

    AbstractType::Ptr resultType;
    for (uint32_t j=subrange.start; j<subrange.end; j++) {
        const ZigNode caseNode = node.extraDataAsNode(j);
        ZigNode caseValue = {caseNode.ast, caseNode.data().rhs};
        ExpressionVisitor valueVisitor(this);
        valueVisitor.setInferredType(inferredType());
        valueVisitor.startVisiting(caseValue, caseNode);
        if (auto builtin = valueVisitor.lastType().dynamicCast<BuiltinType>()) {
            if (builtin->isTrap() || builtin->isUnreachable())
                continue; // skip @panic, @compileError, unreachable branches
        }
        if (!resultType.data()) {
            resultType = valueVisitor.lastType();
        } else {
            resultType = Helper::mergeTypes(resultType, valueVisitor.lastType(), context());
        }
    }
    if (resultType.data()) {
        encounter(resultType);
    } else {
        encounterUnknown();
    }

    return Continue;
}


VisitResult ExpressionVisitor::visitArrayType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor typeVisitor(this);
    typeVisitor.startVisiting(rhs, node);

    SliceType::Ptr sliceType(new SliceType);
    sliceType->setElementType(typeVisitor.lastType());

    if (lhs.tag() == NodeTag_number_literal) {
        bool ok;
        int size = lhs.mainToken().toInt(&ok, 0);
        if (ok) {
            sliceType->setDimension(size);
        }
    }

    encounter(sliceType);
    return Continue;
}

VisitResult ExpressionVisitor::visitArrayInit(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    if (auto slice = v.lastType().dynamicCast<SliceType>()) {
        uint32_t n = node.arrayInitCount();
        if (n && static_cast<uint32_t>(slice->dimension()) != n) {
            SliceType::Ptr newSlice(static_cast<SliceType*>(slice->clone()));
            newSlice->setDimension(n);
            encounter(newSlice);
        } else {
            encounter(slice);
        }
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitArrayAccess(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // TODO: It's possible to check the range
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);

    // Ptr is walked automatically
    auto T = Helper::asZigType(v.lastType());
    if (auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
        // c ptr (eg [*]const u8)
        if (ptr->modifiers() & ArrayModifier) {
            encounter(T);
            return Continue;
        }
    }
    if (auto slice = T.dynamicCast<SliceType>()) {
        auto elementType = slice->elementType();

        // Copy const
        if ((slice->modifiers() & AbstractType::ConstModifier)
            && !(elementType->modifiers() & AbstractType::ConstModifier)
        ) {
            AbstractType::Ptr e(elementType->clone());
            e->setModifiers(elementType->modifiers() | AbstractType::ConstModifier);
            elementType = e;
        }

        // If element is a char/u8 copy comptime known value
        auto v = elementType.dynamicCast<BuiltinType>();
        if (slice->isComptimeKnown() && (slice->dimension() > 0) && v && v->isChar()) {
            ExpressionVisitor v2(this);
            v2.setInferredType(BuiltinType::newFromName(QStringLiteral("usize")));
            v2.startVisiting(node.rhsAsNode(), node);

            auto index = v2.lastType().dynamicCast<BuiltinType>();
            if (index && index->isComptimeKnown() && index->isUnsigned()) {
                bool ok;
                const auto i = index->comptimeKnownValue().str().toULongLong(&ok, 0);
                if (ok) {
                    QString str = slice->comptimeKnownValue().str();
                    if (i < static_cast<qulonglong>(str.size())) {
                        BuiltinType::Ptr e(static_cast<BuiltinType*>(v->clone()));
                        e->setComptimeKnownValue(str.at(i));
                        elementType = e;
                    }
                }
            }
        }

        // TODO: if index is comptime known set
        encounter(elementType);
    } else if (auto vector = T.dynamicCast<VectorType>()) {
         encounter(vector->elementType());
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitArrayCat(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};

    ExpressionVisitor v1(this);
    v1.startVisiting(lhs, node);
    auto A = v1.lastType();
    ExpressionVisitor v2(this);
    v2.startVisiting(rhs, node);
    auto B = v2.lastType();

    bool wrap_in_ptr = false;
    if (auto aPtr = v1.lastType().dynamicCast<PointerType>()) {
        if (auto bPtr = v2.lastType().dynamicCast<PointerType>()) {
            wrap_in_ptr = true;
            A = aPtr->baseType();
            B = bPtr->baseType();
        }
    }

    if (auto a = A.dynamicCast<SliceType>()) {
        if (auto b = B.dynamicCast<SliceType>()) {
            if (
                Helper::typesEqualIgnoringModifiers(a->elementType(), b->elementType())
                && a->dimension() > 0
                && b->dimension() > 0
            ) {
                SliceType::Ptr sliceType(static_cast<SliceType*>(a->clone()));
                sliceType->setDimension(a->dimension() + b->dimension());
                if (a->isComptimeKnown() && b->isComptimeKnown()) {
                    sliceType->setComptimeKnownValue(
                        a->comptimeKnownValue().str() + b->comptimeKnownValue().str()
                    );
                } else {
                    sliceType->clearComptimeValue();
                }
                if (wrap_in_ptr) {
                    PointerType::Ptr ptrType(new PointerType);
                    ptrType->setBaseType(sliceType);
                    encounter(ptrType);
                } else {
                    encounter(sliceType);
                }
                return Continue;
            }
        }
    }


    encounterUnknown();
    return Continue;
}



VisitResult ExpressionVisitor::visitForRange(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    SliceType::Ptr newSlice(new SliceType);
    newSlice->setElementType(BuiltinType::newFromName(QStringLiteral("usize")));
    encounter(newSlice);
    return Continue;
}

VisitResult ExpressionVisitor::visitSlice(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    // Ptr is walked automatically
    auto T = v.lastType();
    if (auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
    }
    if (auto slice = T.dynamicCast<SliceType>()) {
        SliceType::Ptr newSlice(new SliceType);
        newSlice->setElementType(slice->elementType());
        // TODO: Size try to detect size?
        encounter(newSlice); // New slice
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitArrayTypeSentinel(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ZigNode lhs = node.lhsAsNode();
    auto sentinel = ast_array_type_sentinel(node.ast, node.index);
    ZigNode elemType = {node.ast, sentinel.elem_type};

    ExpressionVisitor typeVisitor(this);
    typeVisitor.startVisiting(elemType, node);

    SliceType::Ptr sliceType(new SliceType);
    sliceType->setElementType(typeVisitor.lastType());

    ZigNode sentinelType = {node.ast, sentinel.sentinel};
    if (sentinelType.tag() == NodeTag_number_literal) {
        bool ok;
        QString value = sentinelType.mainToken();
        int size = value.toInt(&ok, 0);
        if (ok) {
            sliceType->setSentinel(size);
        }
    }

    if (lhs.tag() == NodeTag_number_literal) {
        bool ok;
        QString value = lhs.mainToken();
        int size = value.toInt(&ok, 0);
        if (ok) {
            sliceType->setDimension(size);
        }
    }

    encounter(sliceType);
    return Continue;
}


VisitResult ExpressionVisitor::visitFnProto(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.fnName();
    Declaration* decl = nullptr;
    if (!name.isEmpty()) {
        decl = Helper::declarationForName(
            name,
            CursorInRevision::invalid(),
            DUChainPointer<const DUContext>(context())
        );
    }
    if (decl) {
        encounterLvalue(DeclarationPointer(decl));
    } else {
        FunctionType::Ptr fn(new FunctionType);

        const auto n = node.fnParamCount();
        for (uint32_t i=0; i < n; i++) {
            auto paramData = node.fnParamData(i);
            ZigNode paramType = {node.ast, paramData.type_expr};
            ExpressionVisitor v(this);
            v.startVisiting(paramType, node);
            fn->addArgument(v.lastType(), i);
        }

        {
            ExpressionVisitor v(this);
            v.startVisiting(node.returnType(), node);
            fn->setReturnType(v.lastType());
        }
        encounter(fn);
    }
    return Continue;
}

AbstractType::Ptr ExpressionVisitor::functionCallSelfType(
    const ZigNode &owner, const ZigNode &callNode)
{
    // Check if a bound fn
    if (owner.tag() == NodeTag_field_access) {
        ExpressionVisitor ownerVisitor(this);
        ownerVisitor.startVisiting(owner.lhsAsNode(), callNode);
        auto maybeSelfType = Helper::unwrapPointer(ownerVisitor.lastType());

        // Self
        if (maybeSelfType.dynamicCast<StructureType>()
            || maybeSelfType.dynamicCast<EnumType>()
        ) {
            return maybeSelfType;
        }
    }
    return AbstractType::Ptr();
}


} // End namespce
