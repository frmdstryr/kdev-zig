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
        visitChildren(node, parent);
        return Continue;
    case NodeTag_switch:
    case NodeTag_switch_comma:
        return visitSwitch(node, parent);
    //case NodeTag_switch_case:
    //case NodeTag_switch_case_inline:
    case NodeTag_grouped_expression:
    case NodeTag_block:
    case NodeTag_block_semicolon:
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

VisitResult ExpressionVisitor::visitPointerType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    PtrTypeData ptr_info = ast_ptr_type_data(node.ast, node.index);

    ZigNode childType = {node.ast, ptr_info.child_type};
    ExpressionVisitor v(this);
    v.startVisiting(childType, node);

    auto baseType = v.lastType();
    if (ptr_info.info.is_const && !(baseType->modifiers() & AbstractType::ConstModifier)) {
        auto clone = baseType->clone();
        clone->setModifiers(clone->modifiers() | AbstractType::ConstModifier);
        baseType = AbstractType::Ptr(clone);
    }

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
    if (mainToken == QLatin1String("[")) {
        // TODO: slice alignment
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
        PointerType::Ptr ptrType(new PointerType);
        ptrType->setBaseType(baseType);
        if (align > 0) {
            ptrType->setAlignOf(align);
        }
        if (ptr_info.info.is_volatile) {
            ptrType->setModifiers(AbstractType::VolatileModifier);
        }
        auto next_token = ast_node_main_token(node.ast, node.index) + 1;
        if (node.tokenSlice(next_token) == QLatin1String("]")) {
            ptrType->setModifiers(ptrType->modifiers() | ArrayModifier);
        }
        encounter(ptrType);
    } else {
        // Can this happen ?
        PointerType::Ptr ptrType(new PointerType);
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
    sliceType->setElementType(BuiltinType::newFromName("u8"));
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

    SliceType::Ptr sliceType(new SliceType);
    sliceType->setSentinel(0);
    // sliceType->setDimension();
    sliceType->setElementType(BuiltinType::newFromName("u8"));
    sliceType->setModifiers(AbstractType::ConstModifier);
    // sliceType->setValue(node.mainToken()); // Join all lines?

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
    BuiltinType::Ptr t(new BuiltinType(tok.contains(".") ? "comptime_float" : "comptime_int"));
    t->setComptimeKnownValue(tok);
    encounter(t);
    return Continue;
}

VisitResult ExpressionVisitor::visitCharLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    BuiltinType::Ptr t(new BuiltinType("u8"));
    t->setComptimeKnownValue(node.mainToken().at(1)); // Remove the ''
    encounter(t);
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
    } else {
        Declaration* decl = Helper::declarationForName(
            name,
            CursorInRevision::invalid(),
            DUChainPointer<const DUContext>(context())
        );
        if (decl) {
            // DUChainReadLocker lock; // For debug statement only
            // qCDebug(KDEV_ZIG) << "result" << decl->toString();;
            // qCDebug(KDEV_ZIG) << "ident" << name << "found";
            encounterLvalue(DeclarationPointer(decl));
        } else {
            // qCDebug(KDEV_ZIG) << "ident" << name << "unknown";
            encounterUnknown();
        }
    }
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
            encounter(BuiltinType::newFromName("usize"));
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

    auto childContexts = context()->childContexts();
    if (!childContexts.isEmpty()) {
        auto childContext = childContexts.first();
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
    } else if (name == QLatin1String("@typeInfo")) {
        return callBuiltinTypeInfo(node);
    } else if (name == QLatin1String("@fieldParentPtr")) {
        return callBuiltinFieldParentPtr(node);
    } else if (name == QLatin1String("@field")) {
        return callBuiltinField(node);
    } else if (name == QLatin1String("@intFromFloat")) {
        return callBuiltinIntFromFloat(node);
    } else if (name == QLatin1String("@floatFromInt")) {
        return callBuiltinFloatFromInt(node);
    } else if (name == QLatin1String("@intCast")) {
        return callBuiltinIntCast(node);
    } else if (name == QLatin1String("@enumFromInt")) {
        return callBuiltinEnumFromInt(node);
    } else if (name == QLatin1String("@intFromEnum")) {
        return callBuiltinIntFromEnum(node);
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
        auto elemType = BuiltinType::newFromName("u8")->clone();
        elemType->setModifiers(AbstractType::ConstModifier);
        slice->setElementType(AbstractType::Ptr(elemType));
        encounter(AbstractType::Ptr(slice));
    } else if (
        name == QLatin1String("@intFromPtr")
        || name == QLatin1String("@returnAddress")
    ) {
        encounter(BuiltinType::newFromName("usize"));
    } else if (
        name == QLatin1String("@memcpy")
        || name == QLatin1String("@memset")
        || name == QLatin1String("@setCold")
        || name == QLatin1String("@setAlignStack")
        || name == QLatin1String("@setEvalBranchQuota")
        || name == QLatin1String("@setFloatMode")
        || name == QLatin1String("@setRuntimeSafety")
    ) {
        encounter(BuiltinType::newFromName("void"));
    } else if (
        name == QLatin1String("@alignOf")
        || name == QLatin1String("@sizeOf")
        || name == QLatin1String("@bitOffsetOf")
        || name == QLatin1String("@bitSizeOf")
        || name == QLatin1String("@offsetOf")
    ) {
        encounter(BuiltinType::newFromName("comptime_int"));
    } else if (
        name == QLatin1String("@hasField")
        || name == QLatin1String("@hasDecl")
    ) {
        encounter(BuiltinType::newFromName("bool"));
    } else {
        encounterUnknown();
    }
    return Continue;
}

static bool isBuiltinCallTwo(const ZigNode &node)
{
    const NodeTag tag = node.tag();
    return (tag == NodeTag_builtin_call_two || tag == NodeTag_builtin_call_two_comma);
}

VisitResult ExpressionVisitor::callBuiltinIntFromFloat(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isInteger() && isBuiltinCallTwo(node)) {
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
    qCDebug(KDEV_ZIG) << "callBuiltinFloatFromInt";
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isFloat() && isBuiltinCallTwo(node)) {
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

VisitResult ExpressionVisitor::callBuiltinIntCast(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<BuiltinType>();
    if (result && result->isInteger() && isBuiltinCallTwo(node)) {
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

VisitResult ExpressionVisitor::callBuiltinEnumFromInt(const ZigNode &node)
{
    const auto result = inferredType().dynamicCast<EnumType>();
    if (result && isBuiltinCallTwo(node)) {
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
    if (result && result->isInteger() && isBuiltinCallTwo(node)) {
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

VisitResult ExpressionVisitor::callBuiltinTypeInfo(const ZigNode &node)
{
    // TODO: Figure out how to make sure it resolves the imports?
    auto stdBuiltinType = Helper::declarationForImportedModuleName(
        "std.builtin.Type", session()->document().str(), true);
    if (stdBuiltinType) {
        encounterLvalue(DeclarationPointer(stdBuiltinType));
    } else {
        encounterUnknown();
    }
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
        IndexedString doc(importPath);
        Helper::scheduleDependency(doc, session()->jobPriority());
        // Also reschedule reparsing of this
        // qCDebug(KDEV_ZIG) << "Module scheduled for parsing" << importPath.toString();
        // Helper::scheduleDependency(session()->document(), session()->jobPriority());
        DelayedType::Ptr delayedImport(new DelayedType);
        delayedImport->setModifiers(ModuleModifier);
        delayedImport->setIdentifier(doc);
        encounter(delayedImport);
        return Continue;
    }
    encounterUnknown();
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinFieldParentPtr(const ZigNode &node)
{
    ZigNode typeNode = node.nextChild();
    // TODO: This assumes correct arguments
    ExpressionVisitor v(this);
    v.startVisiting(typeNode, node);
    PointerType::Ptr ptr(new PointerType);
    ptr->setBaseType(v.lastType());
    encounter(ptr);
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
    // Shortcut, builtintypes have delayed types
    if (returnType.dynamicCast<BuiltinType>()) {
        encounter(returnType);
        return Continue;
    }

    // Check if any delayed types need resolved
    // It should be able to resolve the type name in the proper context
    // using kdevelops builtin specialization but I'm still not sure
    // how to properly do that, so until then this is a workaround
    const auto &args = func->arguments();
    int startArg = 0;
    if (args.size() > 0) {
        AbstractType::Ptr selfType  = v.functionCallSelfType(next, node);
        if (Helper::baseTypesEqual(args.at(0), selfType)) {
            startArg = 1;
        }
    }

    // eg fn (comptime T: type, ...) []T
    DelayedTypeFinder finder;
    returnType->accept(&finder);
    if (finder.delayedTypes.size() > 0) {
        uint32_t i = 0;
        for (const auto &arg: args.mid(startArg)) {
            if (auto param = arg.dynamicCast<Zig::DelayedType>()) {
                bool found = false;
                Q_ASSERT(!param->identifier().isEmpty());
                for (const auto &t : finder.delayedTypes) {
                    Q_ASSERT(!t->identifier().isEmpty());
                    if (param->identifier() == t->identifier()) {
                        found = true;
                        finder.delayedTypes.removeAll(t);
                        break;
                    }
                }
                if (!found)
                    continue;

                ZigNode argValueNode = node.callParamAt(i);
                if (!argValueNode.isRoot()) {
                    ExpressionVisitor valueVisitor(this);
                    valueVisitor.startVisiting(argValueNode, node);
                    const auto value = valueVisitor.lastType();
                    // WARNING: This does NOT work with empty types
                    SimpleTypeExchanger exchanger(param, value);
                    returnType = exchanger.exchange(AbstractType::Ptr(returnType->clone()));
                }
            }
            i += 1;
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

VisitResult ExpressionVisitor::visitCmpExpr(const ZigNode &node, const ZigNode &parent)
{
    // TODO: eval exprs..
    encounter(BuiltinType::newFromName("bool"));
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
                        encounter(BuiltinType::newFromName("true"));
                    } else {
                        encounter(BuiltinType::newFromName("false"));
                    }
                } else {
                    if (a->isTrue() && b->isTrue()) {
                        encounter(BuiltinType::newFromName("true"));
                    } else {
                        encounter(BuiltinType::newFromName("false"));
                    }
                }
                return Continue;
            }

            // Both types are only runtime known
            encounter(BuiltinType::newFromName("bool"));
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
                encounter(BuiltinType::newFromName("false"));
            } else if (a->isFalse()) {
                encounter(BuiltinType::newFromName("true"));
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

    ExpressionVisitor v1(this);
    v1.startVisiting(lhs, node);

    // Check if lhs & rhs are compatable
    if (const auto a = v1.lastType().dynamicCast<BuiltinType>()) {
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(this);
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
                if (value.at(0) == '-') {
                    result->setComptimeKnownValue(value.mid(1));
                } else {
                    result->setComptimeKnownValue('-' + value);
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
        encounter(errorType->baseType());
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

    auto elseContext = context();
    if (if_data.error_token != INVALID_TOKEN) {
        auto range = node.tokenRange(if_data.error_token);
        DUChainReadLocker lock;
        elseContext = context()->findContextAt(range.end);
    }
    ExpressionVisitor elseExpr(this, elseContext);

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
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ExpressionVisitor v(this);
    v.startVisiting(lhs, node);
    if (const auto switchValue = dynamic_cast<ComptimeType*>(v.lastType().data())) {
        if (switchValue->isComptimeKnown()) {
            // {
            //     DUChainReadLocker lock;
            //     qCDebug(KDEV_ZIG) << "switch is comptime known" << switchValue->toString();
            // }
            // Try to return matching case
            const NodeSubRange subrange = node.subRange();
            if (!subrange.isValid()) {
                qCDebug(KDEV_ZIG) << "switch subrange is invalid" << node.index;
                encounterUnknown();
                return Continue;
            }
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
    // TODO: Merge result types of switch ?
    encounterUnknown();
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
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    //ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v(this);
    v.startVisiting(lhs, node);
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
    // ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    if (auto slice = v.lastType().dynamicCast<SliceType>()) {
        encounter(slice->elementType());
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitForRange(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    SliceType::Ptr newSlice(new SliceType);
    newSlice->setElementType(BuiltinType::newFromName("usize"));
    encounter(newSlice);
    return Continue;
}

VisitResult ExpressionVisitor::visitSlice(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    v.startVisiting(node.lhsAsNode(), node);
    if (auto slice = v.lastType().dynamicCast<SliceType>()) {
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

AbstractType::Ptr ExpressionVisitor::functionCallSelfType(
    const ZigNode &owner, const ZigNode &callNode)
{
    // Check if a bound fn
    if (owner.tag() == NodeTag_field_access) {
        ExpressionVisitor ownerVisitor(this);
        ownerVisitor.startVisiting(owner.lhsAsNode(), callNode);
        auto maybeSelfType = ownerVisitor.lastType();

        // *Self
        if (auto ptr = maybeSelfType.dynamicCast<PointerType>()) {
            maybeSelfType = ptr->baseType();
        }

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
