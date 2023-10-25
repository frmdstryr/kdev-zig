/*
 * Copyright 2017  Emma Gospodinova <emma.gospodinova@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "zignode.h"

namespace Zig
{


NodeKind ZigNode::kind()
{
    return ast_node_kind(ast, index);
}

ZigNode ZigNode::nextChild()
{
    return ZigNode{ast, ast_visit_one_child(ast, index)};
}


SourceRange ZigNode::extent()
{
    return ast_node_extent(ast, index);
}

QString ZigNode::spellingName()
{
    ZigString name = ZigString(ast_node_new_spelling_name(ast, index));
    QString result = (*name == nullptr) ? "" : QString::fromUtf8(*name);
    return result;
}

template<typename ZigObjectType, void (*ZigDestructor)(ZigObjectType *)>
ZigAllocatedObject<ZigObjectType, ZigDestructor>::ZigAllocatedObject(ZigObjectType *object)
    : object(object)
{
}

template<typename ZigObjectType, void (*ZigDestructor)(ZigObjectType *)>
ZigAllocatedObject<ZigObjectType, ZigDestructor>::~ZigAllocatedObject()
{
    ZigDestructor(object);
    object = nullptr;
}

template<typename ZigObjectType, void (*ZigDestructor)(ZigObjectType *)>
ZigObjectType *ZigAllocatedObject<ZigObjectType, ZigDestructor>::data()
{
    return object;
}

template<typename ZigObjectType, void (*ZigDestructor)(ZigObjectType *)>
ZigObjectType *ZigAllocatedObject<ZigObjectType, ZigDestructor>::operator *()
{
    return object;
}

}
