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

#ifndef ZIGNODE_H
#define ZIGNODE_H

#include <language/duchain/ducontext.h>

#include "kdevzigastparser.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

template <typename ZigObjectType, void (*ZigDestructor)(ZigObjectType *)>
class KDEVZIGDUCHAIN_EXPORT ZigAllocatedObject
{
public:
    ZigAllocatedObject(ZigObjectType *object);
    ~ZigAllocatedObject();

    ZigObjectType *data();
    ZigObjectType *operator *();

private:
    ZigObjectType *object;
};

template <typename T> void noop_destructor(T *) {}

using ZigAst = ZigAllocatedObject<ZAst, destroy_ast>;
using ZigString = ZigAllocatedObject<const char, destroy_string>;
using ZigError = ZigAllocatedObject<ZError, destroy_error>;

struct KDEVZIGDUCHAIN_EXPORT ZigNode
{
    ZAst* ast;
    uint32_t index;

    NodeKind kind() const;
    NodeTag tag() const;
    NodeData data() const;
    ZigNode nextChild() const;
    SourceRange extent() const;
    QString spellingName() const;
    QString mainToken() const;
    // Create an anon name for a container
    QString containerName() const;
    QString tokenSlice(TokenIndex i) const;

    // Capture name in case of if/while/for etc
    QString captureName(CaptureType capture) const;
    KDevelop::RangeInRevision captureRange(CaptureType capture) const;

    // Get type node if kind is VarDecl or return root node
    ZigNode varType() const;
    ZigNode varValue() const;

    // Get return node if kind is FuncDecl or return root node
    ZigNode returnType() const;
    bool returnsInferredError() const;
    uint32_t paramCount() const;
    ZigNode paramType(uint32_t i) const;
    QString paramName(uint32_t i) const;
    KDevelop::RangeInRevision paramRange(TokenIndex i) const;
    KDevelop::RangeInRevision tokenRange(TokenIndex i) const;
    KDevelop::RangeInRevision range() const;
    KDevelop::RangeInRevision mainTokenRange() const;

    inline bool isRoot() const { return index == 0; }
};

template class KDEVZIGDUCHAIN_EXPORT ZigAllocatedObject<const char, destroy_string>;
template class KDEVZIGDUCHAIN_EXPORT ZigAllocatedObject<ZAst, destroy_ast>;
template class KDEVZIGDUCHAIN_EXPORT ZigAllocatedObject<ZError, destroy_error>;

}

#endif // ZIGNODE_H
