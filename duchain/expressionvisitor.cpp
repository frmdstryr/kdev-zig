#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/errortype.h"
#include "types/errortype.h"
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
    case NodeTag_optional_type:
        return visitOptionalType(node, parent);
    case NodeTag_string_literal:
        return visitStringLiteral(node, parent);
    case NodeTag_number_literal:
        return visitNumberLiteral(node, parent);
    case NodeTag_ptr_type:
    case NodeTag_ptr_type_aligned:
    case NodeTag_ptr_type_sentinel:
    case NodeTag_ptr_type_bit_range:
        return visitPointerType(node, parent);
    case NodeTag_container_decl:
    case NodeTag_container_decl_arg_trailing:
    case NodeTag_container_decl_two:
    case NodeTag_container_decl_two_trailing:
        return visitContainerDecl(node, parent);
    case NodeTag_error_union:
        return visitErrorUnion(node, parent);
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

} // End namespce
