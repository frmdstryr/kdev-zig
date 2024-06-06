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

ZigNode ZigNode::lhsAsNode() const
{
    return ZigNode{ast, data().lhs};
}

ZigNode ZigNode::rhsAsNode() const
{
    return ZigNode{ast, data().rhs};
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

QString ZigNode::fnName() const
{
    return tokenSlice(ast_fn_name(ast, index));
}

uint32_t ZigNode::fnParamCount() const
{
    return ast_fn_param_count(ast, index);
}

ParamData ZigNode::fnParamData(uint32_t i) const
{
    return ast_fn_param_at(ast, index, i);
}

uint32_t ZigNode::callParamCount() const
{
    return ast_call_arg_count(ast, index);
}

ZigNode ZigNode::callParamAt(uint32_t i) const
{
    return ZigNode{ast, ast_call_arg_at(ast, index, i)};
}

uint32_t ZigNode::structInitCount() const
{
    return ast_struct_init_field_count(ast, index);
}

FieldInitData ZigNode::structInitAt(uint32_t i) const
{
    return ast_struct_init_field_at(ast, index, i);
}

uint32_t ZigNode::arrayInitCount() const
{
    return ast_array_init_item_size(ast, index);
}

ZigNode ZigNode::arrayInitAt(uint32_t i) const
{
    return ZigNode{ast, ast_array_init_item_at(ast, index, i)};
}

uint32_t ZigNode::switchCaseCount() const
{
    return ast_switch_case_size(ast, index);
}

ZigNode ZigNode::switchCaseItemAt(uint32_t i) const
{
    return ZigNode{ast, ast_switch_case_item_at(ast, index, i)};
}

uint32_t ZigNode::forInputCount() const
{
    return ast_for_input_count(ast, index);
}

ZigNode ZigNode::forInputAt(uint32_t i) const
{
    return ZigNode{ast, ast_for_input_at(ast, index, i)};
}


QString ZigNode::tokenSlice(TokenIndex i) const
{
    if (i == INVALID_TOKEN) {
        return QStringLiteral("");
    }
    const auto slice = ast_token_slice(ast, i);
    return QString::fromUtf8(slice.data, slice.len);
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
    return tokenSlice(ast_node_capture_token(ast, index, capture));
}

KDevelop::RangeInRevision ZigNode::captureRange(CaptureType capture) const
{
    return tokenRange(ast_node_capture_token(ast, index, capture));
}

NodeSubRange ZigNode::subRange() const
{
    return ast_sub_range(ast, index);
}

ZigNode ZigNode::extraDataAsNode(uint32_t i) const
{
    return ZigNode{ast, ast_extra_data(ast, i)};
}

QString ZigNode::spellingName() const
{
    TokenIndex tok = ast_node_name_token(ast, index);
    if (tok == INVALID_TOKEN) {
        return QStringLiteral("");
    }
    const auto name = ast_token_slice(ast, tok);
    const char *str = name.data;
    const auto n = name.len;
    if (n > 2 && str[0] == '"' && str[n-1] == '"') {
        str++; // Skip start "
        return QString::fromUtf8(str, n-2); // Trim end "
    }
    return QString::fromUtf8(str, n);
}

QString ZigNode::mainToken() const
{
    return tokenSlice(ast_node_main_token(ast, index));
}

KDevelop::RangeInRevision ZigNode::mainTokenRange() const
{
    return tokenRange(ast_node_main_token(ast, index));
}

KDevelop::RangeInRevision ZigNode::spellingRange() const
{
    return tokenRange(ast_node_name_token(ast, index));
}

QString ZigNode::comment() const
{
    const auto name = ast_node_comment(ast, index);
    if (name.isValid()) {
        return QString::fromUtf8(name.data, name.len);
    }
    return QStringLiteral("");

}

QString ZigNode::containerName() const
{
    return QStringLiteral("anon %1 %2").arg(mainToken()).arg(index);
}

QString ZigNode::blockLabel() const
{
    return tokenSlice(ast_node_block_label_token(ast, index));
}

}
