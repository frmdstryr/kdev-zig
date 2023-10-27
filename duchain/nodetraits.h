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

#ifndef NODETRAITS_H
#define NODETRAITS_H

#include <language/duchain/ducontext.h>

#include "kdevzigastparser.h"

namespace Zig
{

namespace NodeTraits
{

constexpr bool hasContext(NodeKind kind)
{
    return kind == Module
        || kind == ContainerDecl
        || kind == EnumDecl
        || kind == FunctionDecl
        || kind == BlockDecl
        || kind == ErrorDecl
        || kind == VarDecl
        || kind == TestDecl
    ;
}

constexpr KDevelop::DUContext::ContextType contextType(NodeKind kind)
{
    using namespace KDevelop;
    return
        kind == Module           ? DUContext::Namespace
        :  kind == ContainerDecl ? DUContext::Namespace
        :  kind == EnumDecl      ? DUContext::Enum
        :  kind == FunctionDecl  ? DUContext::Function // Function is only for arguments
        :  kind == ErrorDecl     ? DUContext::Enum
        :  kind == TestDecl      ? DUContext::Other
        :  kind == BlockDecl     ? DUContext::Other
        :  kind == VarDecl       ? DUContext::Other
        // TODO: Template decl?
        : static_cast<DUContext::ContextType>(-1);
}

constexpr bool isKDevDeclaration(NodeKind kind)
{
    return kind == ParamDecl
        || kind == EnumDecl
        || kind == ErrorDecl
        || kind == VarDecl
        || kind == TestDecl
        || kind == Module;
}

constexpr bool isTypeDeclaration(NodeKind kind)
{
    return  kind == EnumDecl
        || kind == ErrorDecl
        || kind == ContainerDecl;
}

}

}

#endif // NODETRAITS_H
