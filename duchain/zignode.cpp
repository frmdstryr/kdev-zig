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


NodeKind ZigNode::kind() const
{
    return ast_node_kind(ast, index);
}

NodeTag ZigNode::tag() const
{
    return ast_node_tag(ast, index);
}

NodeData ZigNode::data() const
{
    return ast_node_data(ast, index);
}


ZigNode ZigNode::nextChild() const
{
    return ZigNode{ast, ast_visit_one_child(ast, index)};
}

ZigNode ZigNode::varType() const
{
    return ZigNode{ast, ast_var_type(ast, index)};
}

ZigNode ZigNode::varValue() const
{
    return ZigNode{ast, ast_var_value(ast, index)};
}

ZigNode ZigNode::returnType() const
{
    return ZigNode{ast, ast_fn_return_type(ast, index)};
}

bool ZigNode::returnsInferredError() const
{
    return ast_fn_returns_inferred_error(ast, index);
}

uint32_t ZigNode::paramCount() const
{
    return ast_fn_param_count(ast, index);
}

ZigNode ZigNode::paramType(uint32_t i) const
{
    return ZigNode{ast, ast_fn_param_at(ast, index, i)};
}

QString ZigNode::paramName(uint32_t i) const
{
    TokenIndex tok = ast_fn_param_token(ast, index, i);
    return tokenSlice(tok);
}

KDevelop::RangeInRevision ZigNode::paramRange(uint32_t i) const
{
    TokenIndex tok = ast_fn_param_token(ast, index, i);
    return tokenRange(tok);
}

QString ZigNode::tokenSlice(TokenIndex i) const
{
    if (i == INVALID_TOKEN) {
        return "";
    }
    ZigString name = ZigString(ast_token_slice(ast, i));
    return QString::fromUtf8(*name);
}

KDevelop::RangeInRevision ZigNode::tokenRange(uint32_t i) const
{
    SourceRange range = ast_token_range(ast, i);
    KTextEditor::Range r = range.isEmpty() ?
    KTextEditor::Range::invalid() : KTextEditor::Range(
            range.start.line,
            range.start.column,
            range.end.line,
            range.end.column
        );
    return KDevelop::RangeInRevision::castFromSimpleRange(r);
}

KDevelop::RangeInRevision ZigNode::range() const
{
    SourceRange range = ast_node_range(ast, index);
    KTextEditor::Range r = range.isEmpty() ?
    KTextEditor::Range::invalid() : KTextEditor::Range(
            range.start.line,
            range.start.column,
            range.end.line,
            range.end.column
        );
    return KDevelop::RangeInRevision::castFromSimpleRange(r);
}

SourceRange ZigNode::extent() const
{
    return ast_node_range(ast, index);
}

QString ZigNode::captureName(CaptureType capture) const
{
    TokenIndex tok = ast_node_capture_token(ast, index, capture);
    return tokenSlice(tok);
}

KDevelop::RangeInRevision ZigNode::captureRange(CaptureType capture) const
{
    TokenIndex tok = ast_node_capture_token(ast, index, capture);
    return tokenRange(tok);
}


QString ZigNode::spellingName() const
{
    TokenIndex tok = ast_node_name_token(ast, index);
    if (tok == INVALID_TOKEN) {
        return "";
    }
    ZigString name = ZigString(ast_token_slice(ast, tok));
    const char *str = name.data();
    const auto n = strlen(str);
    if (n > 2 && str[0] == '"' && str[n-1] == '"') {
        str++; // Skip start "
        return QString::fromUtf8(str, n-2); // Trim end "
    }
    return QString::fromUtf8(str);
}

QString ZigNode::mainToken() const
{
    TokenIndex tok = ast_node_main_token(ast, index);
    return tokenSlice(tok);
}

KDevelop::RangeInRevision ZigNode::mainTokenRange() const
{
    TokenIndex tok = ast_node_main_token(ast, index);
    return tokenRange(tok);
}

KDevelop::RangeInRevision ZigNode::spellingRange() const
{
    TokenIndex tok = ast_node_name_token(ast, index);
    return tokenRange(tok);
}

QString ZigNode::comment() const
{
    ZigString name = ZigString(ast_node_comment(ast, index));
    if (name.data()) {
        return QString::fromUtf8(name.data());
    }
    return "";

}


QString ZigNode::containerName() const
{
    if (kind() == ContainerDecl) {
        return QString("anon %1 %2").arg(mainToken()).arg(index);
    }
    return "";
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
