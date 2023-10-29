#pragma once

#include "zignode.h"

namespace Zig
{

class KDEVZIGDUCHAIN_EXPORT Visitor
{
public:
    Visitor() = default;
    virtual ~Visitor() = default;

    // Subclass must override
    virtual VisitResult visitNode(ZigNode &node, ZigNode &parent) = 0;

    // For root node == parent
    virtual void visitChildren(ZigNode &node, ZigNode &parent);


};

}
