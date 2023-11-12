#include <language/duchain/types/integraltype.h>
#include "functionvisitor.h"
#include "expressionvisitor.h"
#include "zigdebug.h"

namespace Zig
{
using namespace KDevelop;

static VisitResult functionVisitorCallback(ZAst* ast, NodeIndex node, NodeIndex parent, void *data)
{
    FunctionVisitor *visitor = static_cast<FunctionVisitor *>(data);
    Q_ASSERT(visitor);
    ZigNode childNode = {ast, node};
    ZigNode parentNode = {ast, parent};
    return visitor->visitNode(childNode, parentNode);
}

FunctionVisitor::FunctionVisitor(ParseSession* session, const DUContext* context)
    : m_context(context), m_session(session)
{
    Q_ASSERT(m_context);
    Q_ASSERT(m_session);
}

FunctionVisitor::~FunctionVisitor()
{
}

AbstractType::Ptr FunctionVisitor::unknownType() const
{
    return AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed));
}

void FunctionVisitor::startVisiting(const ZigNode &node, const ZigNode &parent)
{
    Q_ASSERT(!node.isRoot());
    Q_ASSERT(parent.tag() == NodeTag_fn_decl);
    visitNode(node, parent);
}

void FunctionVisitor::visitChildren(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    ast_visit(node.ast, node.index, functionVisitorCallback, this);
}

VisitResult FunctionVisitor::visitNode(const ZigNode &node, const ZigNode &parent)
{
    NodeTag tag = node.tag();
    // qCDebug(KDEV_ZIG) << "Function visitor node" << node.index << "tag" << tag;
    switch (tag) {
    case NodeTag_return:
        return visitReturn(node, parent);
    default:
        visitChildren(node, parent);
        return Continue;
    }
    return Recurse;
}

VisitResult FunctionVisitor::visitReturn(const ZigNode &node, const ZigNode &parent)
{
    NodeData data = node.data();
    if (data.lhs) {
        ZigNode lhs = {node.ast, data.lhs};
        ExpressionVisitor v(session(), context());
        Q_ASSERT(m_currentFunction);
        v.setCurrentFunction(m_currentFunction);
        v.startVisiting(lhs, node);
        encounterReturn(v.lastType());
    } else {
        // Return void ?
    }
    return Continue;
}

}
