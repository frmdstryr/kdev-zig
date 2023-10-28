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
    default:
        return ContextBuilder::visitNode(node, parent);
    }
}

// This is invoked within the nodes context
void DeclarationBuilder::visitChildren(ZigNode &node, ZigNode &parent)
{
    // NodeTag ptag = parent.tag();
    NodeKind kind = node.kind();
    if (kind == FunctionDecl) {
        updateFunctionDecl(node);
    }
    else if (kind == If || kind == For || kind == While) {
        maybeBuildCapture(node, parent);
    }
    ContextBuilder::visitChildren(node, parent);
}

template<NodeKind Kind>
VisitResult DeclarationBuilder::buildDeclaration(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(parent);
    constexpr bool hasContext = NodeTraits::hasContext(Kind);

    QString name = node.spellingName();
    auto range = editorFindSpellingRange(node, name);
    createDeclaration<Kind>(node, name, hasContext, range);
    VisitResult ret = buildContext<Kind>(node, parent);
    if (hasContext) eventuallyAssignInternalContext();
    closeDeclaration();
    return ret;
}

template <NodeKind Kind>
Declaration *DeclarationBuilder::createDeclaration(ZigNode &node, const QString &name, bool hasContext, KDevelop::RangeInRevision &range)
{
    DUChainWriteLocker lock;

    // qDebug()  << "Create decl node:" << node.index << " name:" << name << " range:" << range  << " kind:" << Kind;
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
    return FunctionType::Ptr(new FunctionType);
}

template <NodeKind Kind, EnableIf<!NodeTraits::isTypeDeclaration(Kind) && Kind != FunctionDecl>>
AbstractType::Ptr DeclarationBuilder::createType(ZigNode &node)
{
    if (Kind == VarDecl || Kind == ParamDecl || Kind == FieldDecl) {
        // Simple var types...
        ZigNode typeNode = Kind == ParamDecl ? node : node.varType();
        // TODO: visit type node expression and return last type?
        if (!typeNode.isRoot()) {
            QString typeName = typeNode.spellingName();
            if (auto builtinType = BuiltinType::newFromName(typeName)) {
                return AbstractType::Ptr(builtinType);
            }

            if (!typeName.isEmpty()) {

            }
        }
    }
    // TODO: Determine var type for
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
    // qDebug() << "Set type struct" << type->toString();
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

// void DeclarationBuilder::updateDeclaration(ZigNode &node)
// {
// //     if (parentDecl && Kind == ContainerDecl && parent.kind() == VarDecl) {
// //         // qDebug() << "Update last decl" << parentDecl->toString();
// //         if (parentDecl && hasCurrentType()) {
// //             DUChainWriteLocker lock;
// //             parentDecl->setIsTypeAlias(true);
// //             parentDecl->setKind(Declaration::Type);
// //             parentDecl->setType(currentAbstractType());
// //         }
// //     }
//     if (node.kind() == FunctionDecl) {
//         updateFunctionDeclaration(node);
//     }
// }

void DeclarationBuilder::updateFunctionDecl(ZigNode &node)
{
    DUChainWriteLocker lock;
    Q_ASSERT(hasCurrentDeclaration());
    auto *decl = currentDeclaration();
    auto fn = decl->type<FunctionType>();
    Q_ASSERT(fn);
    const auto n = node.paramCount();

    if (n > 0) {
        // TODO: Find a better way to do this
        // params get visited more than once but there is also stuff like
        // comptime, linksection, etc
        QString name = node.spellingName();
        for (uint32_t i=0; i < n; i++) {
            ZigNode paramType = node.paramType(i);
            Q_ASSERT(!paramType.isRoot());
            QString paramName = node.paramName(i);
            auto paramRange = node.paramRange(i);
            auto *param = createDeclaration<ParamDecl>(paramType, paramName, true, paramRange);
            fn->addArgument(param->abstractType(), i);
            VisitResult ret = buildContext<ParamDecl>(paramType, node);
            eventuallyAssignInternalContext();
            closeDeclaration();
        }
    }

    ZigNode typeNode = node.returnType();
    Q_ASSERT(!typeNode.isRoot());
    // Handle builtin return types...
    if (auto builtinType = BuiltinType::newFromName(typeNode.spellingName())) {
        fn->setReturnType(AbstractType::Ptr(builtinType));
    }
    decl->setAbstractType(fn);
    // else TODO the rest
}

void DeclarationBuilder::maybeBuildCapture(ZigNode &node, ZigNode &parent)
{
    QString captureName = node.captureName(CaptureType::Payload);
    if (!captureName.isEmpty()) {
        auto range = node.captureRange(CaptureType::Payload);
        // FIXME: Get type node of capture ???
        auto *param = createDeclaration<VarDecl>(node, captureName, true, range);
        closeDeclaration();
    }
}



} // namespace zig
