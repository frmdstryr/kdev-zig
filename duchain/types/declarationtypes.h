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

#ifndef DECLARATIONTYPES_H
#define DECLARATIONTYPES_H

#include <type_traits>

#include <language/duchain/types/abstracttype.h>
#include <language/duchain/types/alltypes.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/classfunctiondeclaration.h>
#include <language/duchain/classmemberdeclaration.h>
#include <language/duchain/aliasdeclaration.h>

#include "kdevzigastparser.h"
#include "nodetraits.h"

namespace Zig
{

template <NodeKind Kind, class Enable = void>
struct IdType;

// template <ZNodeKind Kind>
// struct IdType<Kind, typename std::enable_if<Kind == ContainerDecl>::type>
// {
//     typedef KDevelop::StructureType Type;
// };

template <NodeKind Kind>
struct IdType<Kind, typename std::enable_if<Kind == AliasDecl>::type>
{
    typedef KDevelop::TypeAliasType Type;
};

template <NodeKind Kind>
struct IdType<Kind, typename std::enable_if<Kind == EnumDecl>::type>
{
    typedef KDevelop::EnumerationType Type;
};

template <NodeKind Kind>
struct IdType<Kind, typename std::enable_if<Kind == ErrorDecl>::type>
{
    typedef KDevelop::EnumerationType Type;
};

template <NodeKind Kind, class Enable = void>
struct DeclType;

template <NodeKind Kind>
struct DeclType<Kind, typename std::enable_if<NodeTraits::isKDevDeclaration(Kind)>::type>
{
    typedef KDevelop::Declaration Type;
};


template <NodeKind Kind>
struct DeclType<Kind, typename std::enable_if<Kind == Module || Kind == ContainerDecl>::type>
{
    typedef KDevelop::ClassDeclaration Type;
};

template <NodeKind Kind>
struct DeclType<Kind, typename std::enable_if<Kind == AliasDecl>::type>
{
    typedef KDevelop::AliasDeclaration Type;
};

template <NodeKind Kind>
struct DeclType<Kind, typename std::enable_if<Kind == TemplateDecl>::type>
{
   typedef KDevelop::ClassFunctionDeclaration Type;
};

template <NodeKind Kind>
struct DeclType<Kind, typename std::enable_if<Kind == FunctionDecl>::type>
{
    typedef KDevelop::FunctionDeclaration Type;
};

template <NodeKind Kind>
struct DeclType<Kind, typename std::enable_if<Kind == FieldDecl>::type>
{
    typedef KDevelop::ClassMemberDeclaration Type;
};

}

#endif // DECLARATIONTYPES_H
