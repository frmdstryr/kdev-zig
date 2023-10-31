#include <language/duchain/types/functiontype.h>
#include <language/duchain/duchainlock.h>
#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/errortype.h"
#include "types/slicetype.h"
#include "expressionvisitor.h"
#include "helpers.h"

namespace Zig
{

//QHash<QString, KDevelop::AbstractType::Ptr> ExpressionVisitor::m_defaultTypes;
static VisitResult expressionVistorCallback(ZAst* ast, NodeIndex node, NodeIndex parent, void *data)
{
    ExpressionVisitor *visitor = static_cast<ExpressionVisitor *>(data);
    Q_ASSERT(visitor);
    ZigNode childNode = {ast, node};
    ZigNode parentNode = {ast, parent};
    return visitor->visitNode(childNode, parentNode);
}


ExpressionVisitor::ExpressionVisitor(KDevelop::DUContext* context)
    : DynamicLanguageExpressionVisitor(context)
{
    // if ( m_defaultTypes.isEmpty() ) {
    //     m_defaultTypes.insert("void", AbstractType::Ptr(new BuiltinType("void")));
    //     m_defaultTypes.insert("true", AbstractType::Ptr(new BuiltinType("bool")));
    //     m_defaultTypes.insert("false", AbstractType::Ptr(new BuiltinType("bool")));
    //     m_defaultTypes.insert("comptime_int", AbstractType::Ptr(new BuiltinType("comptime_int")));
    //     m_defaultTypes.insert("comptime_float", AbstractType::Ptr(new BuiltinType("comptime_float")));
    //     m_defaultTypes.insert("string", AbstractType::Ptr(new BuiltinType("[]const u8")));
    // }
}

ExpressionVisitor::ExpressionVisitor(ExpressionVisitor* parent, const KDevelop::DUContext* overrideContext)
    : DynamicLanguageExpressionVisitor(parent)
{
    if ( overrideContext ) {
        m_context = overrideContext;
    }
    Q_ASSERT(context());
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
    // qDebug() << "ExpressionVisitor::visitNode" << node.index;

    switch (node.tag()) {
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
    case NodeTag_error_union:
        return visitErrorUnion(node, parent);
    case NodeTag_builtin_call:
    case NodeTag_builtin_call_comma:
    case NodeTag_builtin_call_two:
    case NodeTag_builtin_call_two_comma:
    case NodeTag_call:
    case NodeTag_call_comma:
    case NodeTag_call_one:
    case NodeTag_call_one_comma:
    case NodeTag_async_call:
    case NodeTag_async_call_comma:
    case NodeTag_async_call_one:
    case NodeTag_async_call_one_comma:
        return visitCall(node, parent);
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
    // qDebug() << "visit pointer";
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
    // qDebug() << "visit address of";
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
    // qDebug() << "visit deref";
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
    // qDebug() << "visit optional";
    Q_UNUSED(parent);
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    auto optionalType = new OptionalType();
    Q_ASSERT(optionalType);
    optionalType->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(optionalType));
    return Recurse;
}

VisitResult ExpressionVisitor::visitUnwrapOptional(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    // qDebug() << "visit unwrap optional";
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
    // TODO: Should it create a new type *const [len:0]u8 for each??
    encounter(AbstractType::Ptr(new BuiltinType("*const [:0]u8")));
    return Recurse;
}

VisitResult ExpressionVisitor::visitNumberLiteral(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.spellingName();
    // qDebug() << "visit number lit" << name;
    encounter(AbstractType::Ptr(
        new BuiltinType(name.contains(".") ?
            "comptime_float" : "comptime_int")));
    return Recurse;
}

VisitResult ExpressionVisitor::visitIdentifier(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    QString name = node.spellingName();
    // qDebug() << "visit ident" << name;
    if (BuiltinType::isBuiltinType(name)) {
        encounter(AbstractType::Ptr(new BuiltinType(name)));
    } else if (name == "true" || name == "false") {
        encounter(AbstractType::Ptr(new BuiltinType("bool")));
    } else {
        CursorInRevision findBeforeCursor = CursorInRevision::invalid();
        Declaration* decl = Helper::declarationForName(
            name,
            findBeforeCursor,
            DUChainPointer<const DUContext>(context())
        );
        if (decl) {
            encounter(decl->abstractType(), DeclarationPointer(decl));
        } else {
            encounterUnknown();
        }
    }

    return Recurse;
}

VisitResult ExpressionVisitor::visitContainerDecl(ZigNode &node, ZigNode &parent)
{
    NodeKind kind = parent.kind();
    if (kind == VarDecl) {
        QString name = parent.spellingName();
        CursorInRevision findBeforeCursor = CursorInRevision::invalid();
        Declaration* decl = Helper::declarationForName(
            name,
            findBeforeCursor,
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

VisitResult ExpressionVisitor::visitCall(ZigNode &node, ZigNode &parent)
{
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
    ExpressionVisitor v(this);
    ZigNode owner = node.nextChild();
    v.visitNode(owner, node);
    QString attr = node.tokenSlice(node.data().rhs);
    DUChainReadLocker lock;
    if (auto *decl = Helper::accessAttribute(v.lastType(), attr, topContext())) {
        if (auto ptr = decl->abstractType().dynamicCast<PointerType>()) {
            // Pointers are automtically accessed
            encounter(ptr->baseType(), DeclarationPointer(decl));
        } else {
            encounter(decl->abstractType(), DeclarationPointer(decl));
        }
    } else {
        encounterUnknown();
    }
    return Recurse;
}

VisitResult ExpressionVisitor::visitErrorUnion(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    // qDebug() << "visit pointer";
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
    Q_UNUSED(parent);
    encounter(AbstractType::Ptr(new BuiltinType("bool")));
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
        encounter(AbstractType::Ptr(ptrType));
    } else {
        encounterUnknown();
    }


    return Recurse;
}

} // End namespce
