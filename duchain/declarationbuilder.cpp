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

#include "declarationbuilder.h"

#include <type_traits>
#include <QRegularExpression>

#include <language/duchain/duchainlock.h>

#include "types/builtintype.h"
#include "types/declarationtypes.h"
#include "nodetraits.h"
#include "zigdebug.h"

namespace Zig
{

using namespace KDevelop;

VisitResult DeclarationBuilder::visitNode(ZigNode &node, ZigNode &parent)
{
    switch (node.kind()) {
    case Module:
        return buildDeclaration<Module>(node, parent);
    case ContainerDecl:
        return buildDeclaration<ContainerDecl>(node, parent);
    case EnumDecl:
        return buildDeclaration<EnumDecl>(node, parent);
    case TemplateDecl:
        return buildDeclaration<TemplateDecl>(node, parent);
    case FunctionDecl:
        return buildDeclaration<FunctionDecl>(node, parent);
    case AliasDecl:
        return buildDeclaration<AliasDecl>(node, parent);
    case FieldDecl:
        return buildDeclaration<FieldDecl>(node, parent);
    case VarDecl:
        return buildDeclaration<VarDecl>(node, parent);
    case ErrorDecl:
        return buildDeclaration<ErrorDecl>(node, parent);
    case TestDecl:
        return buildDeclaration<TestDecl>(node, parent);
    case Ident:
        return updateDeclaration<Ident>(node, parent);
    default:
        return ContextBuilder::visitNode(node, parent);
    }
}

template<NodeKind Kind>
VisitResult DeclarationBuilder::buildDeclaration(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    constexpr bool hasContext = NodeTraits::hasContext(Kind);

    QString name = node.spellingName();

    createDeclaration<Kind>(node, name, hasContext);
    // qDebug() << "Open decl" << name.value;
    VisitResult ret = buildContext<Kind>(node, parent);
    if (hasContext) eventuallyAssignInternalContext();
    // qDebug() << "Close decl" << name.value;
    closeDeclaration();
    return ret;
}

template <NodeKind Kind>
Declaration *DeclarationBuilder::createDeclaration(ZigNode &node, const QString &name, bool hasContext)
{

    auto range = editorFindSpellingRange(node, name);
    DUChainWriteLocker lock;

    // qDebug()  << "Create decl node:" << node.index << " name:" << name->value << " range:" << range  << " kind:" << Kind;
    typename DeclType<Kind>::Type *decl = openDeclaration<typename DeclType<Kind>::Type>(
        Identifier(name), range,
        hasContext ? DeclarationIsDefinition : NoFlags
    );

    if (NodeTraits::isTypeDeclaration(Kind)) {
        decl->setKind(Declaration::Type);
    }

    auto type = createType<Kind>(node);
    openType(type);

    setDeclData<Kind>(decl);
    setType<Kind>(decl, type.data());
    closeType();

    return decl;
}

template <NodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
typename IdType<Kind>::Type::Ptr DeclarationBuilder::createType(ZigNode &node)
{
    Q_UNUSED(node);
    return typename IdType<Kind>::Type::Ptr(new typename IdType<Kind>::Type);
}

template <NodeKind Kind, EnableIf<Kind == ContainerDecl>>
StructureType::Ptr DeclarationBuilder::createType(ZigNode &node)
{
    Q_UNUSED(node); // TODO: Determine container type (struct, union)
    return StructureType::Ptr(new StructureType);
}

template <NodeKind Kind, EnableIf<Kind == FunctionDecl>>
FunctionType::Ptr DeclarationBuilder::createType(ZigNode &node)
{
    Q_UNUSED(node); // TODO determine method in struct or regular method
    return FunctionType::Ptr(new FunctionType);
}

template <NodeKind Kind, EnableIf<!NodeTraits::isTypeDeclaration(Kind) && Kind != FunctionDecl>>
AbstractType::Ptr DeclarationBuilder::createType(ZigNode &node)
{
    Q_UNUSED(node);
    // TODO: Determine var type
    return AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed));
}

template <NodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, typename IdType<Kind>::Type *type)
{
    setType<Kind>(decl, static_cast<IdentifiedType *>(type));
    setType<Kind>(decl, static_cast<AbstractType *>(type));
}

template <NodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, IdentifiedType *type)
{
    type->setDeclaration(decl);
}

template <NodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, AbstractType *type)
{
    decl->setAbstractType(AbstractType::Ptr(type));
}

template <NodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, StructureType *type)
{
    type->setDeclaration(decl);
}

template<NodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
void DeclarationBuilder::setDeclData(ClassDeclaration *decl)
{
    if (Kind == ContainerDecl) {
        decl->setClassType(ClassDeclarationData::Struct);
    }
}

template<NodeKind Kind, EnableIf<Kind == Module>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    decl->setKind(Declaration::Namespace);
}

template<NodeKind Kind, EnableIf<Kind == VarDecl>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    decl->setKind(Declaration::Instance);
}

template<NodeKind Kind, EnableIf<Kind == AliasDecl>>
void DeclarationBuilder::setDeclData(AliasDeclaration *decl)
{
    decl->setIsTypeAlias(true);
    decl->setKind(Declaration::Type);
}

template<NodeKind Kind, EnableIf<Kind != VarDecl && Kind != Module>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    Q_UNUSED(decl);
}

template<NodeKind Kind>
VisitResult DeclarationBuilder::updateDeclaration(ZigNode &node, ZigNode &parent)
{
    // ZNodeKind parentKind = ast_node_kind(parent);
    if (Kind == Ident && lastType() && hasCurrentDeclaration()) {
        auto type = lastType().dynamicCast<FunctionType>();
        auto decl = dynamic_cast<FunctionDeclaration*>(currentDeclaration());
        if (type && decl) {
            QString name = node.spellingName();
            DUChainWriteLocker lock;
            if (auto builtinType = BuiltinType::newFromName(name)) {
                // qDebug() << "fn return type is" << returnType->toString();
                type->setReturnType(AbstractType::Ptr(builtinType));
                decl->setAbstractType(type);
            } else {
                QualifiedIdentifier typeName((Identifier(name)));
                QList<Declaration*> declarations = findSimpleVar(typeName, currentContext());
                if (!declarations.isEmpty()) {
                    Declaration* decl = declarations.first();
                    type->setReturnType(decl->abstractType());
                    decl->setAbstractType(type);
                } // else might not be defined yet
            }
        }
    }
    return ContextBuilder::visitNode(node, parent);
}



} // namespace zig
