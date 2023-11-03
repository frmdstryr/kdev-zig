#include <language/duchain/types/structuretype.h>
#include <language/duchain/types/functiontype.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/duchainutils.h>
#include <language/duchain/duchain.h>
#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/errortype.h"
#include "types/slicetype.h"
#include "expressionvisitor.h"
#include "helpers.h"
#include "zigdebug.h"

namespace Zig
{

static VisitResult expressionVistorCallback(ZAst* ast, NodeIndex node, NodeIndex parent, void *data)
{
    ExpressionVisitor *visitor = static_cast<ExpressionVisitor *>(data);
    Q_ASSERT(visitor);
    ZigNode childNode = {ast, node};
    ZigNode parentNode = {ast, parent};
    return visitor->visitNode(childNode, parentNode);
}


ExpressionVisitor::ExpressionVisitor(ParseSession* session, KDevelop::DUContext* context)
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


void ExpressionVisitor::visitChildren(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    ast_visit(node.ast, node.index, expressionVistorCallback, this);
}

void ExpressionVisitor::startVisiting(ZigNode &node, ZigNode &parent)
{
    Q_ASSERT(!node.isRoot());
    ast_visit(node.ast, node.index, expressionVistorCallback, this);
    visitNode(node, parent);
}

VisitResult ExpressionVisitor::visitNode(ZigNode &node, ZigNode &parent)
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
    case NodeTag_string_literal:
        return visitStringLiteral(node, parent);
    case NodeTag_number_literal:
        return visitNumberLiteral(node, parent);
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
        return visitBoolExpr(node, parent);
    case NodeTag_try:
        return visitTry(node, parent);
    case NodeTag_array_type:
        return visitArrayType(node, parent);
    case NodeTag_array_access:
        return visitArrayAccess(node, parent);
    case NodeTag_slice:
    case NodeTag_slice_open:
    case NodeTag_slice_sentinel:
        return visitSlice(node, parent);
    case NodeTag_array_type_sentinel:
        return visitArrayTypeSentinel(node, parent);
    case NodeTag_ptr_type_aligned:
        return visitPointerTypeAligned(node, parent);
    //case NodeTag_ptr_type_sentinel:
    default:
        break;
    }
    return Recurse;
}

VisitResult ExpressionVisitor::visitPointerType(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitAddressOf(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitDeref(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}


VisitResult ExpressionVisitor::visitOptionalType(ZigNode &node, ZigNode &parent)
{
    // qCDebug(KDEV_ZIG) << "visit optional";
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    auto optionalType = new OptionalType();
    optionalType->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(optionalType));
    return Recurse;
}

VisitResult ExpressionVisitor::visitUnwrapOptional(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    // qCDebug(KDEV_ZIG) << "visit unwrap optional";
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    if (auto optional = v.lastType().dynamicCast<OptionalType>()) {
        encounter(AbstractType::Ptr(optional->baseType()));
    } else {
        encounterUnknown(); // TODO: Set error?
    }
    return Recurse;

}

VisitResult ExpressionVisitor::visitStringLiteral(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitNumberLiteral(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.spellingName();
    // qCDebug(KDEV_ZIG) << "visit number lit" << name;
    encounter(BuiltinType::newFromName(
        name.contains(".") ? "comptime_float" : "comptime_int"));
    return Recurse;
}

VisitResult ExpressionVisitor::visitIdentifier(ZigNode &node, ZigNode &parent)
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
            encounter(decl->abstractType(), DeclarationPointer(decl));
        } else {
            // Needs resolved later
            auto delayedType = new DelayedType();
            delayedType->setIdentifier(IndexedTypeIdentifier(name));
            encounter(AbstractType::Ptr(delayedType));
        }
    }

    return Recurse;
}

VisitResult ExpressionVisitor::visitStructInit(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}


VisitResult ExpressionVisitor::visitContainerDecl(ZigNode &node, ZigNode &parent)
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
            encounter(decl->abstractType(), DeclarationPointer(decl));
        } else {
            encounterUnknown();
        }
    } else {
        encounterUnknown();
    }
    return Recurse;
}

VisitResult ExpressionVisitor::visitBuiltinCall(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.mainToken();
    if (name == "@This") {
        return callBuiltinThis(node);
    } else if (name == "@import") {
       return callBuiltinImport(node);
    // todo @Type
    } else if (
        // These return the type of the first argument
        name == "@as"
        || name == "@sqrt"
        || name == "@sin"
        || name == "@cos"
        || name == "@tan"
        || name == "@exp"
        || name == "@exp2"
        || name == "@log"
        || name == "@log2"
        || name == "@log10"
        || name == "@floor"
        || name == "@ceil"
        || name == "@trunc"
        || name == "@round"
        || name == "@min"
        || name == "@max"
        || name == "@mod"
        || name == "@rem"
        || name == "@abs"
        || name == "@shlExact"
        || name == "@shrExact"
        || name == "@mulAdd"
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
        name == "@errorName"
        || name == "@tagName"
        || name == "@typeName"
        || name == "@embedFile"
    ) {
        auto slice = new SliceType();
        slice->setSentinel(0);
        auto elemType = new BuiltinType("u8");
        elemType->setModifiers(BuiltinType::CommonModifiers::ConstModifier);
        slice->setElementType(AbstractType::Ptr(elemType));
        encounter(AbstractType::Ptr(slice));
    } else if (
        name == "@intFromPtr"
        || name == "@returnAddress"
    ) {
        encounter(BuiltinType::newFromName("usize"));
    } else if (
        name == "@memcpy"
        || name == "@memset"
        || name == "@setCold"
        || name == "@setAlignStack"
        || name == "@setEvalBranchQuota"
        || name == "@setFloatMode"
        || name == "@setRuntimeSafety"
    ) {
        encounter(BuiltinType::newFromName("void"));
    } else if (
        name == "@alignOf"
        || name == "@sizeOf"
        || name == "@bitOffsetOf"
        || name == "@bitSizeOf"
        || name == "@offsetOf"
    ) {
        encounter(BuiltinType::newFromName("comptime_int"));
    } else {
        encounterUnknown();
    }
    return Recurse;
}

VisitResult ExpressionVisitor::callBuiltinThis(ZigNode &node)
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

VisitResult ExpressionVisitor::callBuiltinImport(ZigNode &node)
{
    // TODO: Report problem if invalid argument or file does not exist
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
            qCDebug(KDEV_ZIG) << "Imported module " << mod->toString() << "from" << importPath.toString();
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

VisitResult ExpressionVisitor::visitCall(ZigNode &node, ZigNode &parent)
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
        encounter(func->returnType());
    } else {
        // TODO: Handle builtins?
        encounterUnknown();
    }
    return Recurse;
}

VisitResult ExpressionVisitor::visitFieldAccess(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    ZigNode owner = node.nextChild();
    v.visitNode(owner, node);
    QString attr = node.tokenSlice(node.data().rhs);
    const auto T = v.lastType();
    // Root modules have a ModuleModifier set
    // const auto isModule = T->modifiers() & ModuleModifier;
    // qCDebug(KDEV_ZIG) << "Access " << attr << " on" << (isModule ? "module" : "") << v.lastType()->toString() ;

    // if (auto delayed = v.lastType().dynamicCast<DelayedType>()) {
    //     QString ident = delayed->identifier().toString();
    //     qDebug() << "Access delayed type" << ident;
    //     auto decl = declarationForName(
    //         ident,
    //         CursorInRevision::invalid(),
    //         DUChainPointer<const DUContext>(topContext)
    //     );
    //     if (decl) {
    //         return accessAttribute(decl->abstractType(), attribute, topContext);
    //     }
    // }

    if (auto s = T.dynamicCast<SliceType>()) {
        // Slices have builtin len / ptr types
        if (attr == "len") {
            encounter(BuiltinType::newFromName("usize"));
        } else if (attr == "ptr") {
            auto ptr = new PointerType();
            ptr->setModifiers(s->modifiers());
            ptr->setBaseType(s->elementType());
            encounter(AbstractType::Ptr(ptr));
        } else {
            encounterUnknown();
        }
        return Recurse;
    }

    if (auto *decl = Helper::accessAttribute(T, attr, topContext())) {
        // DUChainReadLocker lock; // Needed if printing debug statement
        // qCDebug(KDEV_ZIG) << " result " << decl->abstractType()->toString();
        encounterLvalue(DeclarationPointer(decl));
    }
    else {
        // qCDebug(KDEV_ZIG) << " no result ";
        encounterUnknown();
    }
    return Recurse;
}

VisitResult ExpressionVisitor::visitErrorUnion(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}


VisitResult ExpressionVisitor::visitBoolExpr(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    encounter(BuiltinType::newFromName("bool"));
    return Recurse;
}

VisitResult ExpressionVisitor::visitTry(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitArrayType(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitArrayAccess(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitSlice(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitArrayTypeSentinel(ZigNode &node, ZigNode &parent)
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
    return Recurse;
}

VisitResult ExpressionVisitor::visitPointerTypeAligned(ZigNode &node, ZigNode &parent)
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
    if (mainToken == "[") {
        // TODO: slice alignment
        auto sliceType = new SliceType();
        Q_ASSERT(sliceType);
        sliceType->setElementType(typeVisitor.lastType());
        encounter(AbstractType::Ptr(sliceType));
    } else if (mainToken == "*") {
        auto ptrType = new PointerType();
        Q_ASSERT(ptrType);
        ptrType->setBaseType(typeVisitor.lastType());
        if (ok) {
            ptrType->setAlignOf(align);
        }
        auto next_token = ast_node_main_token(node.ast, node.index) + 1;
        if (node.tokenSlice(next_token) == "]") {
            ptrType->setModifiers(ArrayModifier);
        }
        encounter(AbstractType::Ptr(ptrType));
    } else {
        encounterUnknown();
    }


    return Recurse;
}

} // End namespce
