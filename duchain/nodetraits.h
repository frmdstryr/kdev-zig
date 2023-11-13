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
    return kind == ContainerDecl
        || kind == EnumDecl
        || kind == FunctionDecl
        || kind == BlockDecl
        || kind == ErrorDecl
        || kind == TestDecl
        || kind == If
        || kind == For
        || kind == While
        || kind == Switch
        || kind == Defer
        //|| kind == Module // Module uses top context
        // var/field need to have internal context if they use @import
        || kind == VarDecl
        || kind == FieldDecl
    ;
}

constexpr bool hasChildren(NodeKind kind)
{
    return hasContext(kind) || (kind == Module); // || kind == FieldDecl || kind == Module);
}

constexpr KDevelop::DUContext::ContextType contextType(NodeKind kind)
{
    using namespace KDevelop;
    return
        kind == Module            ? DUContext::Namespace
        :  kind == ContainerDecl  ? DUContext::Class
        :  kind == EnumDecl       ? DUContext::Enum
        :  kind == FunctionDecl   ? DUContext::Function // Function is only for arguments
        :  kind == ErrorDecl      ? DUContext::Enum
        :  kind == TestDecl       ? DUContext::Other
        :  kind == BlockDecl      ? DUContext::Other
        :  kind == VarDecl        ? DUContext::Other
        :  kind == FieldDecl      ? DUContext::Other
        :  kind == If             ? DUContext::Other
        :  kind == For            ? DUContext::Other
        :  kind == While          ? DUContext::Other
        :  kind == Switch         ? DUContext::Other
        :  kind == Defer          ? DUContext::Other

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
    ;
}

constexpr bool isTypeDeclaration(NodeKind kind)
{
    return  kind == EnumDecl
        || kind == ErrorDecl
        || kind == ContainerDecl
        || kind == Module
    ;
}

// Set the owner context. Decls that can be returned by @This();
constexpr bool shouldSetContextOwner(NodeKind kind)
{
    return (kind == Module
        || kind == ContainerDecl
        || kind == EnumDecl
    );
}

constexpr bool canHaveCapture(NodeKind kind)
{
    return  kind == If
        || kind == For
        || kind == While
        || kind == Defer
        || kind == Catch;
}

constexpr bool shouldSetComment(NodeKind kind)
{
    return kind == ContainerDecl
        || kind == VarDecl
        || kind == FieldDecl
        || kind == FunctionDecl
    ;

}

constexpr bool shouldUseParentName(NodeKind kind, NodeKind parentKind)
{
    return parentKind == VarDecl && (
        kind == ContainerDecl
        || kind == EnumDecl
        || kind == ErrorDecl
        // || kind == Module
    );
}



} // nodetraits

} // zig

#endif // NODETRAITS_H
