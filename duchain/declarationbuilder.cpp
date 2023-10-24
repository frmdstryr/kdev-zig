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

ZVisitResult DeclarationBuilder::visitNode(ZNode &node, ZNode &parent)
{
    ZNodeKind kind = ast_node_kind(node);
    switch (kind) {
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

template<ZNodeKind Kind>
ZVisitResult DeclarationBuilder::buildDeclaration(ZNode &node, ZNode &parent)
{
    Q_UNUSED(parent);
    constexpr bool hasContext = NodeTraits::hasContext(Kind);

    ZigPath name(node);

    DUChainWriteLocker lock(DUChain::lock());
    createDeclaration<Kind>(node, &name, hasContext);
    // qDebug() << "Open decl" << name.value;
    ZVisitResult ret = buildContext<Kind>(node, parent);
    if (hasContext) eventuallyAssignInternalContext();
    // qDebug() << "Close decl" << name.value;
    closeDeclaration();
    return ret;
}

template <ZNodeKind Kind>
Declaration *DeclarationBuilder::createDeclaration(ZNode &node, ZigPath *name, bool hasContext)
{

    auto range = editorFindSpellingRange(node, name->value);

    // qDebug()  << "Create decl node:" << node.index << " name:" << name->value << " range:" << range  << " kind:" << Kind;
    typename DeclType<Kind>::Type *decl = openDeclaration<typename DeclType<Kind>::Type>(
        Identifier(name->value), range,
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

template <ZNodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
typename IdType<Kind>::Type::Ptr DeclarationBuilder::createType(ZNode &node)
{
    Q_UNUSED(node);
    return typename IdType<Kind>::Type::Ptr(new typename IdType<Kind>::Type);
}

template <ZNodeKind Kind, EnableIf<Kind == ContainerDecl>>
StructureType::Ptr DeclarationBuilder::createType(ZNode &node)
{
    Q_UNUSED(node); // TODO: Determine container type (struct, union)
    return StructureType::Ptr(new StructureType);
}

template <ZNodeKind Kind, EnableIf<Kind == FunctionDecl>>
FunctionType::Ptr DeclarationBuilder::createType(ZNode &node)
{
    Q_UNUSED(node); // TODO determine method in struct or regular method
    return FunctionType::Ptr(new FunctionType);
}

template <ZNodeKind Kind, EnableIf<!NodeTraits::isTypeDeclaration(Kind) && Kind != FunctionDecl>>
AbstractType::Ptr DeclarationBuilder::createType(ZNode &node)
{
    Q_UNUSED(node);
    // TODO: Determine var type
    return AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed));
}

template <ZNodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, typename IdType<Kind>::Type *type)
{
    setType<Kind>(decl, static_cast<IdentifiedType *>(type));
    setType<Kind>(decl, static_cast<AbstractType *>(type));
}

template <ZNodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, IdentifiedType *type)
{
    type->setDeclaration(decl);
}

template <ZNodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, AbstractType *type)
{
    decl->setAbstractType(AbstractType::Ptr(type));
}

template <ZNodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, StructureType *type)
{
    type->setDeclaration(decl);
}

template<ZNodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
void DeclarationBuilder::setDeclData(ClassDeclaration *decl)
{
    if (Kind == ContainerDecl) {
        decl->setClassType(ClassDeclarationData::Struct);
    }
}

template<ZNodeKind Kind, EnableIf<Kind == Module>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    decl->setKind(Declaration::Namespace);
}

template<ZNodeKind Kind, EnableIf<Kind == VarDecl>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    decl->setKind(Declaration::Instance);
}

template<ZNodeKind Kind, EnableIf<Kind == AliasDecl>>
void DeclarationBuilder::setDeclData(AliasDeclaration *decl)
{
    decl->setIsTypeAlias(true);
    decl->setKind(Declaration::Type);
}

template<ZNodeKind Kind, EnableIf<Kind != VarDecl && Kind != Module>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    Q_UNUSED(decl);
}

template<ZNodeKind Kind>
ZVisitResult DeclarationBuilder::updateDeclaration(ZNode &node, ZNode &parent)
{
    // ZNodeKind parentKind = ast_node_kind(parent);
    if (Kind == Ident && lastType() && hasCurrentDeclaration()) {
        auto type = lastType().dynamicCast<FunctionType>();
        auto decl = dynamic_cast<FunctionDeclaration*>(currentDeclaration());
        if (type && decl) {
            ZigPath name(node);
            if (auto builtinType = findBuiltinType(name)) {
                DUChainWriteLocker lock;
                // qDebug() << "fn return type is" << returnType->toString();
                type->setReturnType(AbstractType::Ptr(builtinType));
                decl->setAbstractType(type);
            } else {
                QList<Declaration*> declarations;
                {
                    DUChainReadLocker lock;
                    QualifiedIdentifier typeName(Identifier(name.value));
                    declarations = findSimpleVar(typeName, currentContext());
                }
                if (!declarations.isEmpty()) {
                    Declaration* decl = declarations.first();
                    DUChainWriteLocker lock;
                    type->setReturnType(decl->abstractType());
                    decl->setAbstractType(type);
                } // else might not be defined yet
            }
        }
    }
    return ContextBuilder::visitNode(node, parent);
}

BuiltinType* DeclarationBuilder::findBuiltinType(const ZigPath &name) const
{
    static QRegularExpression unsignedIntPattern("u\\d+");
    static QRegularExpression signedIntPattern("i\\d+");
    static QRegularExpression floatPattern("f(16|32|64|80|128)");
    if (name.value == "void")
        return new BuiltinType("void");
    if (name.value == "bool")
        return new BuiltinType("bool");
    if (name.value == "isize")
        return new BuiltinType("isize");
    if (name.value == "usize")
        return new BuiltinType("usize");
    if (name.value == "type")
        return new BuiltinType("type");
    if (name.value == "anyerror")
        return new BuiltinType("anyerror");
    if (name.value == "noreturn")
        return new BuiltinType("noreturn");
    if (name.value == "anyopaque")
        return new BuiltinType("anyopaque");
    if (name.value == "comptime_int")
        return new BuiltinType("comptime_int");
    if (name.value == "comptime_float")
        return new BuiltinType("comptime_float");
    if (unsignedIntPattern.match(name.value).hasMatch())
        return new BuiltinType(name.value);
    if (signedIntPattern.match(name.value).hasMatch())
        return new BuiltinType(name.value);
    if (floatPattern.match(name.value).hasMatch())
        return new BuiltinType(name.value);
    // TODO: c_ types
    return nullptr;
}

} // namespace zig
