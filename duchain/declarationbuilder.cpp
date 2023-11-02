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

#include "expressionvisitor.h"
#include "types/builtintype.h"
#include "types/errortype.h"
#include "types/declarationtypes.h"
#include "nodetraits.h"
#include "helpers.h"
#include "zigdebug.h"

namespace Zig
{

using namespace KDevelop;

VisitResult DeclarationBuilder::visitNode(ZigNode &node, ZigNode &parent)
{
    NodeKind kind = node.kind();
    // qDebug() << "DeclarationBuilder::visitNode" << node.index << "kind" << kind;
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
    default:
        return ContextBuilder::visitNode(node, parent);
    }
}

// This is invoked within the nodes context
void DeclarationBuilder::visitChildren(ZigNode &node, ZigNode &parent)
{
    // qDebug() << "DeclarationBuilder::visitChildren" << node.index;
    NodeKind kind = node.kind();

    if (NodeTraits::shouldSetContextOwner(kind)) {
        DUChainWriteLocker lock;
        currentContext()->setOwner(currentDeclaration());
    }

    if (kind == FunctionDecl) {
        updateFunctionDecl(node);
    }
    else if (NodeTraits::canHaveCapture(kind)) {
        maybeBuildCapture(node, parent);
    }
    ContextBuilder::visitChildren(node, parent);


}

template<NodeKind Kind>
VisitResult DeclarationBuilder::buildDeclaration(ZigNode &node, ZigNode &parent)
{
    if (shouldSkipNode(node, parent)) {
        return Recurse; // Skip this case
    }
    Q_UNUSED(parent);
    constexpr bool hasContext = NodeTraits::hasContext(Kind);
    bool overwrite = NodeTraits::shouldUseParentName(Kind, parent.kind());
    QString name = overwrite ? parent.spellingName() : node.spellingName();
    auto range = editorFindSpellingRange(overwrite ? parent : node, name);
    createDeclaration<Kind>(node, parent, name, hasContext, range);
    VisitResult ret = buildContext<Kind>(node, parent);
    if (hasContext) {
        eventuallyAssignInternalContext();
    }
    closeDeclaration();
    return ret;
}

template <NodeKind Kind>
Declaration *DeclarationBuilder::createDeclaration(ZigNode &node, ZigNode &parent, const QString &name, bool hasContext, KDevelop::RangeInRevision &range)
{
    DUChainWriteLocker lock;
    Identifier identifier(name);
    auto declRange = Kind == Module ? RangeInRevision::invalid(): range;
    if (Kind == Module) {
        // FIXME: Relative module name?
        QStringList parts = session->document().str().split("/");
        identifier = Identifier("@import " + parts.last().replace(".zig", ""));
    } else if (Kind == TestDecl) {
        if (name.isEmpty()) {
            // include space so it cannot be referenced as a variable
            identifier = Identifier("test 0");
        } else {
            identifier = Identifier("test " + name);
        }
    }

    typename DeclType<Kind>::Type *decl = openDeclaration<typename DeclType<Kind>::Type>(
        identifier,
        declRange,
        hasContext ? DeclarationIsDefinition : NoFlags
    );

    if (NodeTraits::isTypeDeclaration(Kind)) {
        decl->setKind(Declaration::Type);
    }

    auto type = createType<Kind>(node, parent);
    openType(type);
    setDeclData<Kind>(decl);
    setType<Kind>(decl, type.data());
    closeType();
    // qDebug()  << "Create decl node:" << node.index << " name:" << identifier.toString() << " range:" << declRange << " kind:" << Kind << "type" << type->toString();
    return decl;
}

template <NodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
typename IdType<Kind>::Type::Ptr DeclarationBuilder::createType(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    return typename IdType<Kind>::Type::Ptr(new typename IdType<Kind>::Type);
}

template <NodeKind Kind, EnableIf<Kind == Module || Kind == ContainerDecl>>
StructureType::Ptr DeclarationBuilder::createType(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent)
    auto structType = new StructureType();
    Q_ASSERT(structType);
    if (Kind == Module) {
        structType->setModifiers(ModuleModifier);
    }
    return StructureType::Ptr(structType);
}

template <NodeKind Kind, EnableIf<Kind == FunctionDecl>>
FunctionType::Ptr DeclarationBuilder::createType(ZigNode &node, ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    return FunctionType::Ptr(new FunctionType);
}

template <NodeKind Kind, EnableIf<!NodeTraits::isTypeDeclaration(Kind) && Kind != FunctionDecl>>
AbstractType::Ptr DeclarationBuilder::createType(ZigNode &node, ZigNode &parent)
{
    NodeTag parentTag = parent.tag();
    if (
        Kind == FieldDecl
        && parent.kind() == EnumDecl
        && (parentTag == NodeTag_container_decl_arg || parentTag == NodeTag_container_decl_arg_trailing)
    ) {
        // enum(arg)
        ZigNode arg = parent.nextChild();
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(arg, parent);
        return v.lastType();
    }
    else if (Kind == VarDecl || Kind == ParamDecl || Kind == FieldDecl) {
        // const bool isConst = (Kind == VarDecl) ? node.mainToken() == "const" : false;
        ZigNode typeNode = Kind == ParamDecl ? node : node.varType();

        if (!typeNode.isRoot()) {
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(typeNode, node);
            return v.lastType();
        } else {
            ZigNode valueNode = node.varValue();
            if (!valueNode.isRoot()) {
                ExpressionVisitor v(session, currentContext());
                v.startVisiting(valueNode, node);
                return v.lastType();
            }
        }
    }
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
    decl->setAbstractType(AbstractType::Ptr(type));
}

template<NodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
void DeclarationBuilder::setDeclData(ClassDeclaration *decl)
{
    if (Kind == Module || Kind == ContainerDecl) {
        decl->setClassType(ClassDeclarationData::Struct);
    }
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
    decl->setKind(Declaration::Alias);
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
            createDeclaration<ParamDecl>(paramType, node, paramName, true, paramRange);

            //lock.unlock();
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(paramType, node);
            //lock.lock();
            fn->addArgument(v.lastType(), i);

            buildContext<ParamDecl>(paramType, node);
            eventuallyAssignInternalContext();
            closeDeclaration();
            //lock.unlock();
        }
    }

    {
        ZigNode typeNode = node.returnType();
        Q_ASSERT(!typeNode.isRoot());
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(typeNode, node);

        // Zig does not use error_union for inferred return types...
        auto returnType = v.lastType();
        if (node.returnsInferredError()) {
            auto *errType = new ErrorType();
            errType->setBaseType(returnType);
            returnType = AbstractType::Ptr(errType);
        }

        fn->setReturnType(returnType);
        decl->setAbstractType(fn);
    }
}

void DeclarationBuilder::maybeBuildCapture(ZigNode &node, ZigNode &parent)
{
    QString captureName = node.captureName(CaptureType::Payload);
    if (!captureName.isEmpty()) {
        auto range = node.captureRange(CaptureType::Payload);
        // FIXME: Get type node of capture ???
        DUChainWriteLocker lock;
        createDeclaration<VarDecl>(node, parent, captureName, true, range);
        closeDeclaration();
    }
}

} // namespace zig
