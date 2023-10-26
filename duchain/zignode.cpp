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

NodeTag ZigNode::tag()
{
    return ast_node_tag(ast, index);
}

ZigNode ZigNode::nextChild()
{
    return ZigNode{ast, ast_visit_one_child(ast, index)};
}

ZigNode ZigNode::varType()
{
    return ZigNode{ast, ast_var_type(ast, index)};
}

ZigNode ZigNode::returnType()
{
    return ZigNode{ast, ast_fn_return_type(ast, index)};
}

uint32_t ZigNode::paramCount()
{
    return ast_fn_param_count(ast, index);
}

ZigNode ZigNode::paramType(uint32_t i)
{
    return ZigNode{ast, ast_fn_param_at(ast, index, i)};
}

QString ZigNode::paramName(uint32_t i)
{
    ZigString name = ZigString(ast_fn_param_name(ast, index, i));
    QString result = (*name == nullptr) ? "" : QString::fromUtf8(*name);
    return result;
}

KDevelop::RangeInRevision ZigNode::paramNameRange(uint32_t i)
{
    SourceRange range = ast_fn_param_name_range(ast, index, i);
    KTextEditor::Range r = range.isEmpty() ?
    KTextEditor::Range::invalid() : KTextEditor::Range(
            range.start.line,
            range.start.column,
            range.end.line,
            range.end.column
        );
    return KDevelop::RangeInRevision::castFromSimpleRange(r);
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
