#include <language/duchain/types/structuretype.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/types/typealiastype.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/duchain.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/aliasdeclaration.h>
#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/errortype.h"
#include "types/slicetype.h"
#include "expressionvisitor.h"
#include "helpers.h"
#include "zigdebug.h"
#include "nodetraits.h"
#include "types/comptimetype.h"
#include "functionvisitor.h"

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
    , m_lastTopContext(parent->m_lastTopContext)
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
    Q_ASSERT(!node.isRoot());
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
        return visitPointerType(node, parent);
    case NodeTag_container_decl:
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
    case NodeTag_bool_and:
    case NodeTag_bool_not:
        return visitBoolExpr(node, parent);

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
    case NodeTag_ptr_type_aligned:
        return visitPointerTypeAligned(node, parent);
    //case NodeTag_ptr_type_sentinel:
    case NodeTag_if:
    //case NodeTag_if_simple: // not expr?
        return visitIf(node, parent);
    case NodeTag_return:
        visitChildren(node, parent);
        return Continue;
    case NodeTag_switch:
    case NodeTag_switch_comma:
    case NodeTag_switch_case:
    case NodeTag_switch_case_inline:
    case NodeTag_block:
    case NodeTag_block_semicolon:
    case NodeTag_grouped_expression:
    case NodeTag_await:
    case NodeTag_comptime:
        visitChildren(node, parent);
        return Continue;
    }
    // TODO: Thereset

    return Recurse;
}

VisitResult ExpressionVisitor::visitPointerType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit pointer";
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    auto ptrType = new PointerType();
    Q_ASSERT(ptrType);
    ptrType->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(ptrType));
    return Continue;
}

VisitResult ExpressionVisitor::visitAddressOf(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit address of";
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    auto ptrType = new PointerType();
    Q_ASSERT(ptrType);
    ptrType->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(ptrType));
    return Continue;
}

VisitResult ExpressionVisitor::visitDeref(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit deref";
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    if (auto ptr = v.lastType().dynamicCast<PointerType>()) {
        encounter(AbstractType::Ptr(ptr->baseType()));
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
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    auto optionalType = new OptionalType();
    optionalType->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(optionalType));
    return Continue;
}

VisitResult ExpressionVisitor::visitUnwrapOptional(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit unwrap optional";
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);

    // Automatically derefs
    auto T = v.lastType();
    if (auto ptr = T.dynamicCast<PointerType>()) {
        T = ptr->baseType();
    }
    if (auto optional = T.dynamicCast<OptionalType>()) {
        encounter(AbstractType::Ptr(optional->baseType()));
    } else {
        encounterUnknown(); // TODO: Set error?
    }
    return Continue;

}

VisitResult ExpressionVisitor::visitStringLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString value = node.mainToken();

    auto sliceType = new SliceType();
    Q_ASSERT(sliceType);
    sliceType->setSentinel(0);
    sliceType->setDimension(value.size());
    sliceType->setElementType(BuiltinType::newFromName("u8"));
    sliceType->setModifiers(AbstractType::CommonModifiers::ConstModifier);

    auto ptrType = new PointerType();
    Q_ASSERT(ptrType);
    ptrType->setBaseType(AbstractType::Ptr(sliceType));
    encounter(AbstractType::Ptr(ptrType));
    return Continue;
}

VisitResult ExpressionVisitor::visitMultilineStringLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    //auto range = node.range();

    auto sliceType = new SliceType();
    Q_ASSERT(sliceType);
    sliceType->setSentinel(0);
    // sliceType->setDimension();
    sliceType->setElementType(BuiltinType::newFromName("u8"));
    sliceType->setModifiers(AbstractType::CommonModifiers::ConstModifier);

    auto ptrType = new PointerType();
    Q_ASSERT(ptrType);
    ptrType->setBaseType(AbstractType::Ptr(sliceType));
    encounter(AbstractType::Ptr(ptrType));
    return Continue;
}

VisitResult ExpressionVisitor::visitNumberLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.spellingName();
    // qCDebug(KDEV_ZIG) << "visit number lit" << name;
    encounter(BuiltinType::newFromName(
        name.contains(".") ? "comptime_float" : "comptime_int"));
    return Continue;
}

VisitResult ExpressionVisitor::visitCharLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    encounter(BuiltinType::newFromName("u8"));
    return Continue;
}

VisitResult ExpressionVisitor::visitEnumLiteral(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    //QString name = node.spellingName();
    // qCDebug(KDEV_ZIG) << "visit number lit" << name;
    //encounter(BuiltinType::newFromName(
    //    name.contains(".") ? "comptime_float" : "comptime_int"));
    encounterUnknown(); // TODO: Get enum?
    return Continue;
}

VisitResult ExpressionVisitor::visitIdentifier(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.spellingName();
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
            if (auto top = Helper::declarationTopContext(decl)) {
                encounterTopContext(top);
                // {
                //     DUChainReadLocker lock;
                //     qCDebug(KDEV_ZIG) << "found top for" << decl->toString();
                // }
            }
            encounterLvalue(DeclarationPointer(decl));
        } else {
            // qCDebug(KDEV_ZIG) << "ident" << name << "unknown";
            // Needs resolved later
            //auto delayedType = new DelayedType();
            //delayedType->setIdentifier(IndexedTypeIdentifier(name));
            //encounter(AbstractType::Ptr(delayedType));
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
    v.visitNode(owner, node);
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
            auto ptr = new PointerType();
            ptr->setModifiers(s->modifiers());
            ptr->setBaseType(s->elementType());
            encounter(AbstractType::Ptr(ptr));
        } else {
            encounterUnknown();
        }
        return Continue;
    }

    if (auto *decl = Helper::accessAttribute(T, attr, v.lastTopContext())) {
        //DUChainReadLocker lock; // Needed if printing debug statement
        //qCDebug(KDEV_ZIG) << " result " << decl->toString();
        if (auto top = Helper::declarationTopContext(decl)) {
            encounterTopContext(top);
        } else if (v.lastTopContext() != topContext()) {
            encounterTopContext(v.lastTopContext());
        }
        encounterLvalue(DeclarationPointer(decl));
    }
    else {
        //qCDebug(KDEV_ZIG) << " no result ";
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitStructInit(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    ZigNode child = node.nextChild();
    v.startVisiting(child, node);
    if (auto s = v.lastType().dynamicCast<StructureType>()) {
        encounter(s);
    } else {
        encounterUnknown();
    }
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

    DUChainReadLocker lock;
    auto contexts = context()->childContexts();
    if (!contexts.isEmpty()) {
        auto bodyContext = contexts.first();
        auto decls = bodyContext->findLocalDeclarations(Identifier(node.containerName()));
        if (!decls.isEmpty()) {
            encounterLvalue(DeclarationPointer(decls.last()));
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
    if (name == QLatin1String("@This")) {
        return callBuiltinThis(node);
    }
    else if (name == QLatin1String("@import")) {
       return callBuiltinImport(node);
    }
    else if (name == QLatin1String("@fieldParentPtr")) {
        return callBuiltinFieldParentPtr(node);
    }
    // todo @Type
    else if (
        // These return the type of the first argument
        name == QLatin1String("@as")
        || name == QLatin1String("@sqrt")
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
        ZigNode typeNode = node.nextChild();
        if (typeNode.isRoot()) {
            encounterUnknown();
        } else {
            ExpressionVisitor v(this);
            v.startVisiting(typeNode, node);
            encounter(v.lastType());
        }
    } else if (
        name == QLatin1String("@errorName")
        || name == QLatin1String("@tagName")
        || name == QLatin1String("@typeName")
        || name == QLatin1String("@embedFile")
    ) {
        auto slice = new SliceType();
        slice->setSentinel(0);
        auto elemType = BuiltinType::newFromName("u8");
        elemType->setModifiers(BuiltinType::CommonModifiers::ConstModifier);
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
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::callBuiltinThis(const ZigNode &node)
{
    // TODO: Report problem if arguments ?
    auto range = node.range();
    DUChainReadLocker lock;
    if (auto thisCtx = Helper::thisContext(range.start, topContext())) {
        if (auto owner = thisCtx->owner()) {
            encounter(owner->abstractType(), DeclarationPointer(owner));
            return Recurse;
        }
    }
    encounterUnknown();
    return Recurse;
}

VisitResult ExpressionVisitor::callBuiltinImport(const ZigNode &node)
{
    // TODO: Report problem if invalid argument
    ZigNode strNode = node.nextChild();
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

    // TODO: How do I import from another file???
    DUChainReadLocker lock;
    auto *importedModule = DUChain::self()->chainForDocument(importPath);
    if (importedModule) {
        if (auto mod = importedModule->owner()) {
            qCDebug(KDEV_ZIG) << "Imported module " << mod->toString() << "from" << importPath.toString() << "ctx" << importedModule;
            encounterTopContext(importedModule);
            encounterLvalue(DeclarationPointer(mod));
            return Continue;
        }
        qCDebug(KDEV_ZIG) << "Module has no owner" << importPath.toString();
    } else {
        Helper::scheduleDependency(IndexedString(importPath), session()->jobPriority());
        // Also reschedule reparsing of this
        qCDebug(KDEV_ZIG) << "Module scheduled for parsing" << importPath.toString();
        // Helper::scheduleDependency(session()->document(), session()->jobPriority());
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
    auto ptr = new PointerType();
    Q_ASSERT(ptr);
    ptr->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(ptr));
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
    v.visitNode(next, node);
    if (auto func = v.lastType().dynamicCast<FunctionType>()) {
        if (auto comptimeType = func->returnType().dynamicCast<ComptimeType>()) {
            // Chain must be locked the whole time here
            // or the ast ptr may become invalid

            DUChainReadLocker lock;
            auto fnDecl = comptimeType->valueDeclaration();
            if (!fnDecl.isValid() || !comptimeType->ast()) {
                encounterUnknown();
                return Continue;
            }
            ZigNode fnNode = {comptimeType->ast(), comptimeType->node()};
            if (fnNode.tag() != NodeTag_fn_decl) {
                // TODO: Handle fn not in same file
                qCWarning(KDEV_ZIG) << "Invalid fn node when resolving comptime type";
                encounterUnknown();
                return Continue;
            }

            NodeData data = fnNode.data();
            ZigNode bodyNode = {fnNode.ast, data.rhs};
            FunctionVisitor f(session(), fnDecl.declaration()->internalContext());
            // TODO: Fill in arguments?
            f.startVisiting(bodyNode, fnNode);
            //qCDebug(KDEV_ZIG) << "Fn return " << f.returnType()->toString();
            encounter(f.returnType());
            return Continue;
        }
        encounter(func->returnType());
    } else {
        // TODO: Handle builtins?
        encounterUnknown();
    }
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
    errorVisitor.visitNode(lhs, node);
    ExpressionVisitor typeVisitor(this);
    typeVisitor.visitNode(rhs, node);
    auto errType = new ErrorType();
    Q_ASSERT(errType);
    errType->setBaseType(typeVisitor.lastType());
    errType->setErrorType(errorVisitor.lastType());
    encounter(AbstractType::Ptr(errType));
    return Continue;
}


VisitResult ExpressionVisitor::visitBoolExpr(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    encounter(BuiltinType::newFromName("bool"));
    return Continue;
}

VisitResult ExpressionVisitor::visitMathExpr(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};

    ExpressionVisitor v1(this);
    v1.visitNode(lhs, node);

    // Check if lhs & rhs are compatable
    if (auto a = v1.lastType().dynamicCast<BuiltinType>()) {
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(this);
        v2.visitNode(rhs, node);
        if (auto b = v2.lastType().dynamicCast<BuiltinType>()) {
            if ((a->isFloat() && b->isFloat())
                || (a->isSigned() && b->isSigned())
                || (a->isUnsigned() && b->isUnsigned())
            ) {
                encounter(a->isComptime() ? b : a);
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
    ZigNode child = node.nextChild();
    ExpressionVisitor v(this);
    v.visitNode(child, node);
    if (auto a = v.lastType().dynamicCast<BuiltinType>()) {
        if (a->isSigned() || a->isFloat()) {
            encounter(a);
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
    v.visitNode(child, node);
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
    ZigNode next = node.nextChild();
    v.visitNode(next, node);
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
    v1.visitNode(lhs, node);
    if (auto optionalType = v1.lastType().dynamicCast<OptionalType>()) {
        // Returns base type
        ZigNode rhs = {node.ast, data.rhs};
        ExpressionVisitor v2(this);
        v2.visitNode(rhs, node);
        encounter(optionalType->baseType());
        return Continue;
    }
    encounterUnknown(); // TODO: Show error?
    return Continue;
}

VisitResult ExpressionVisitor::visitIf(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    IfData data = ast_if_data(node.ast, node.index);
    if (data.payload_token != 0) {
        ZigNode cond = {node.ast, data.cond_expr};
        ExpressionVisitor v(this);
        v.visitNode(cond, node);
        if (auto optionalType = v.lastType().dynamicCast<OptionalType>()) {
            // Returns base type
            // rhs is only valid for if_simple
            // ZigNode rhs = {node.ast, data.rhs};
            // ExpressionVisitor v2(this);
            // v2.visitNode(rhs, node);
            // TODO: Merge types???
            encounter(optionalType->baseType());
            return Continue;
        }
    }
    else {
        ZigNode then = {node.ast, data.then_expr};
        ExpressionVisitor v1(this);
        v1.visitNode(then, node);
        encounter(v1.lastType());
        // TODO: Check if else type is compatible
        return Continue;
    }
    encounterUnknown(); // TODO: Show error?
    return Continue;

}


VisitResult ExpressionVisitor::visitArrayType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor typeVisitor(this);
    typeVisitor.visitNode(rhs, node);

    auto sliceType = new SliceType();
    Q_ASSERT(sliceType);
    sliceType->setElementType(typeVisitor.lastType());

    if (lhs.tag() == NodeTag_number_literal) {
        bool ok;
        QString value = lhs.mainToken();
        int size = value.toInt(&ok, 0);
        if (ok) {
            sliceType->setDimension(size);
        }
    }

    encounter(AbstractType::Ptr(sliceType));
    return Continue;
}

VisitResult ExpressionVisitor::visitArrayInit(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    //ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v(this);
    v.visitNode(lhs, node);
    if (auto slice = v.lastType().dynamicCast<SliceType>()) {
        uint32_t n = ast_array_init_size(node.ast, node.index);
        if (n && static_cast<uint32_t>(slice->dimension()) != n) {
            auto newSlice = static_cast<SliceType*>(slice->clone());
            Q_ASSERT(newSlice);
            newSlice->setDimension(n);
            encounter(AbstractType::Ptr(newSlice));
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
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    // TODO: It's possible to check the range
    // ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v(this);
    v.startVisiting(lhs, node);

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
    auto newSlice = new SliceType();
    Q_ASSERT(newSlice);
    newSlice->setElementType(BuiltinType::newFromName("usize"));
    encounter(AbstractType::Ptr(newSlice)); // New slice
    return Continue;
}

VisitResult ExpressionVisitor::visitSlice(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    // ZigNode rhs = {node.ast, data.rhs};
    ExpressionVisitor v(this);
    v.startVisiting(lhs, node);

    if (auto slice = v.lastType().dynamicCast<SliceType>()) {
        auto newSlice = new SliceType();
        Q_ASSERT(newSlice);
        newSlice->setElementType(slice->elementType());
        // TODO: Size try to detect size?
        encounter(AbstractType::Ptr(newSlice)); // New slice
    } else {
        encounterUnknown();
    }
    return Continue;
}

VisitResult ExpressionVisitor::visitArrayTypeSentinel(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    auto sentinel = ast_array_type_sentinel(node.ast, node.index);
    ZigNode elemType = {node.ast, sentinel.elem_type};


    ExpressionVisitor typeVisitor(this);
    typeVisitor.visitNode(elemType, node);

    auto sliceType = new SliceType();
    Q_ASSERT(sliceType);
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

    encounter(AbstractType::Ptr(sliceType));
    return Continue;
}

VisitResult ExpressionVisitor::visitPointerTypeAligned(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    NodeData data = node.data();
    ZigNode lhs = {node.ast, data.lhs};
    ZigNode rhs = {node.ast, data.rhs};

    QString mainToken = node.mainToken();

    bool ok;
    int align = -1;
    if (lhs.tag() == NodeTag_number_literal) {
        QString value = lhs.mainToken();
        align = value.toInt(&ok, 0);
    }

    ExpressionVisitor typeVisitor(this);
    typeVisitor.visitNode(rhs, node);
    if (mainToken == QLatin1String("[")) {
        // TODO: slice alignment
        auto sliceType = new SliceType();
        Q_ASSERT(sliceType);
        sliceType->setElementType(typeVisitor.lastType());
        encounter(AbstractType::Ptr(sliceType));
    } else if (mainToken == QLatin1String("*")) {
        auto ptrType = new PointerType();
        Q_ASSERT(ptrType);
        ptrType->setBaseType(typeVisitor.lastType());
        if (ok) {
            ptrType->setAlignOf(align);
        }
        auto next_token = ast_node_main_token(node.ast, node.index) + 1;
        if (node.tokenSlice(next_token) == QLatin1String("]")) {
            ptrType->setModifiers(ArrayModifier);
        }
        encounter(AbstractType::Ptr(ptrType));
    } else {
        encounterUnknown();
    }
    return Continue;
}

} // End namespce
