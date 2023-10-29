#include "visitor.h"

namespace Zig
{

VisitResult visitCallback(ZAst* ast, NodeIndex node, NodeIndex parent, void *data)
{
    Visitor *visitor = static_cast<Visitor *>(data);
    Q_ASSERT(visitor);
    ZigNode childNode = {ast, node};
    ZigNode parentNode = {ast, parent};
    return visitor->visitNode(childNode, parentNode);
}

void Visitor::visitChildren(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    ast_visit(node.ast, node.index, visitCallback, this);
}


} // namespace zig
