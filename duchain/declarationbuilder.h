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

#ifndef DECLARATIONBUILDER_H
#define DECLARATIONBUILDER_H

#include <type_traits>

#include <language/duchain/builders/abstracttypebuilder.h>
#include <language/duchain/builders/abstractdeclarationbuilder.h>
#include <language/duchain/types/alltypes.h>

#include "types/declarationtypes.h"
#include "contextbuilder.h"
#include "nodetraits.h"
#include "zignode.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

using TypeBuilderBase = KDevelop::AbstractTypeBuilder<ZNode, ZigPath, ContextBuilder>;
using DeclarationBuilderBase = KDevelop::AbstractDeclarationBuilder<ZNode, ZigPath, TypeBuilderBase>;

namespace detail { enum class enabler {}; }
constexpr detail::enabler dummy = {};
template <bool Condition>
using EnableIf = typename std::enable_if<Condition, detail::enabler>::type;

class KDEVZIGDUCHAIN_EXPORT DeclarationBuilder : public DeclarationBuilderBase
{
public:
    DeclarationBuilder() = default;
    ~DeclarationBuilder() override = default;

protected:
    ZVisitResult visitNode(ZNode &node, ZNode &parent) override;


private:
    template <ZNodeKind Kind>
    ZVisitResult buildDeclaration(ZNode &node, ZNode &parent);

    template <ZNodeKind Kind>
    KDevelop::Declaration *createDeclaration(ZNode &node, ZigPath *name, bool hasContext);

    template <ZNodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)> = dummy>
    typename IdType<Kind>::Type::Ptr createType(ZNode &node);

    template <ZNodeKind Kind, EnableIf<Kind == FunctionDecl> = dummy>
    KDevelop::FunctionType::Ptr createType(ZNode &node);

    template <ZNodeKind Kind, EnableIf<!NodeTraits::isTypeDeclaration(Kind) && Kind != FunctionDecl> = dummy>
    KDevelop::AbstractType::Ptr createType(ZNode &node);

    template <ZNodeKind Kind, EnableIf<Kind != VarDecl && Kind != Module> = dummy>
    void setDeclData(KDevelop::Declaration *decl);

    template<ZNodeKind Kind, EnableIf<Kind == VarDecl> = dummy>
    void setDeclData(KDevelop::Declaration *decl);

    template<ZNodeKind Kind, EnableIf<Kind == Module> = dummy>
    void setDeclData(KDevelop::Declaration *decl);

    template<ZNodeKind Kind, EnableIf<Kind == AliasDecl> = dummy>
    void setDeclData(KDevelop::AliasDeclaration *decl);

    template <ZNodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)> = dummy>
    void setDeclData(KDevelop::ClassDeclaration *decl);

    template <ZNodeKind Kind>
    void setType(KDevelop::Declaration *decl, typename IdType<Kind>::Type *type);

    template <ZNodeKind Kind>
    void setType(KDevelop::Declaration *decl, KDevelop::IdentifiedType *type);

    template <ZNodeKind Kind>
    void setType(KDevelop::Declaration *decl, KDevelop::AbstractType *type);
};

}

#endif // DECLARATIONBUILDER_H
