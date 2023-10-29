#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "expressionvisitor.h"
#include "helpers.h"

namespace Zig
{

//QHash<QString, KDevelop::AbstractType::Ptr> ExpressionVisitor::m_defaultTypes;

ExpressionVisitor::ExpressionVisitor(KDevelop::DUContext* context)
    : DynamicLanguageExpressionVisitor(context), Visitor()
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
    : DynamicLanguageExpressionVisitor(parent), Visitor()
{
    if ( overrideContext ) {
        m_context = overrideContext;
    }
    Q_ASSERT(context());
}

VisitResult ExpressionVisitor::visitNode(ZigNode &node, ZigNode &parent)
{
    switch (node.tag()) {
    case NodeTag_identifier:
        return visitIdentifier(node);
    case NodeTag_optional_type:
        return visitOptionalType(node);
    case NodeTag_string_literal:
        return visitStringLiteral(node);
    case NodeTag_number_literal:
        return visitNumberLiteral(node);
    case NodeTag_ptr_type:
    case NodeTag_ptr_type_aligned:
    case NodeTag_ptr_type_sentinel:
    case NodeTag_ptr_type_bit_range:
        return visitPointerType(node);
    default:
        visitChildren(node, parent);
    }
    return Recurse;
}

VisitResult ExpressionVisitor::visitPointerType(ZigNode &node)
{
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

VisitResult ExpressionVisitor::visitOptionalType(ZigNode &node)
{
    // qDebug() << "visit optional";
    ExpressionVisitor v(this);
    ZigNode value = node.nextChild();
    v.visitNode(value, node);
    auto optionalType = new OptionalType();
    Q_ASSERT(optionalType);
    optionalType->setBaseType(v.lastType());
    encounter(AbstractType::Ptr(optionalType));
    return Recurse;
}

VisitResult ExpressionVisitor::visitStringLiteral(ZigNode &node)
{
    // TODO: Should it create a new type *const [len:0]u8 for each??
    encounter(AbstractType::Ptr(new BuiltinType("[]const u8")));
    return Continue;
}

VisitResult ExpressionVisitor::visitNumberLiteral(ZigNode &node)
{
    QString name = node.spellingName();
    // qDebug() << "visit number lit" << name;
    encounter(AbstractType::Ptr(
        new BuiltinType(name.contains(".") ?
            "comptime_float" : "comptime_int")));
    return Continue;
}

VisitResult ExpressionVisitor::visitIdentifier(ZigNode &node)
{
    QString name = node.spellingName();
    // qDebug() << "visit ident" << name;
    if (BuiltinType::isBuiltinType(name)) {
        encounter(AbstractType::Ptr(new BuiltinType(name)));
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

    return Continue;
}

} // End namespce
