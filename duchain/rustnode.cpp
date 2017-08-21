#include "rustnode.h"

namespace Rust
{

RustPath::RustPath(RustNode *node)
{
    RustString name = RustString(node_get_spelling_name(node->data()));

    if (*name == nullptr) {
        value = QString();
    } else {
        value = QString::fromUtf8(*name);
    }
}

RustNode::RustNode(RSNode *node)
    : RustAllocatedObject(node)
{
}

RustNode::RustNode(RustOwnedNode &node)
    : RustAllocatedObject(node.data())
{
}

RustNode::~RustNode()
{
}

template<typename RustObjectType, void (*RustDestructor)(RustObjectType *)>
RustAllocatedObject<RustObjectType, RustDestructor>::RustAllocatedObject(RustObjectType *object)
    : object(object)
{
}

template<typename RustObjectType, void (*RustDestructor)(RustObjectType *)>
RustAllocatedObject<RustObjectType, RustDestructor>::~RustAllocatedObject()
{
    RustDestructor(object);
    object = nullptr;
}

template<typename RustObjectType, void (*RustDestructor)(RustObjectType *)>
RustObjectType *RustAllocatedObject<RustObjectType, RustDestructor>::data()
{
    return object;
}

template<typename RustObjectType, void (*RustDestructor)(RustObjectType *)>
RustObjectType *RustAllocatedObject<RustObjectType, RustDestructor>::operator *()
{
    return object;
}

}
