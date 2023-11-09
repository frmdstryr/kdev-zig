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

#include <language/duchain/problem.h>
#include <language/duchain/duchainlock.h>
#include <language/editor/documentrange.h>

#include "expressionvisitor.h"
#include "types/builtintype.h"
#include "types/errortype.h"
#include "types/declarationtypes.h"
#include "nodetraits.h"
#include "helpers.h"
#include "zigdebug.h"
#include "types/optionaltype.h"
#include "types/slicetype.h"
#include "functionvisitor.h"

namespace Zig
{

using namespace KDevelop;

ReferencedTopDUContext DeclarationBuilder::build(
    const IndexedString& url, const ZigNode* node,
    const ReferencedTopDUContext& updateContext)
{
    ReferencedTopDUContext ctx(updateContext);
    //m_correctionHelper.reset(new CorrectionHelper(url, this));

    // The declaration builder needs to run twice,
    // so it can resolve uses of structs, functions, etc
    // which are used before they are defined .
    if ( ! m_prebuilding ) {
        DeclarationBuilder prebuilder;
        prebuilder.setParseSession(session);
        prebuilder.setPrebuilding(true);
        ctx = prebuilder.build(url, node, updateContext);
        qCDebug(KDEV_ZIG) << "Second declarationbuilder pass";
    }
    else {
        qCDebug(KDEV_ZIG) << "Prebuilding declarations";
    }
    return DeclarationBuilderBase::build(url, node, ctx);
}

VisitResult DeclarationBuilder::visitNode(const ZigNode &node, const ZigNode &parent)
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
    case Usingnamespace:
        visitUsingnamespace(node, parent);
        // fall through
    default:
        return ContextBuilder::visitNode(node, parent);
    }
}

// This is invoked within the nodes context
void DeclarationBuilder::visitChildren(const ZigNode &node, const ZigNode &parent)
{
    // qDebug() << "DeclarationBuilder::visitChildren" << node.index;
    NodeKind kind = node.kind();

    if (NodeTraits::shouldSetContextOwner(kind)) {
        DUChainWriteLocker lock;
        currentContext()->setOwner(currentDeclaration());
    }

    switch (kind) {
    case FunctionDecl:
        updateFunctionDeclArgs(node);
        break;
#define BUILD_CAPTURE_FOR(K) case K: maybeBuildCapture<K>(node, parent); break
        BUILD_CAPTURE_FOR(If);
        BUILD_CAPTURE_FOR(For);
        BUILD_CAPTURE_FOR(While);
        BUILD_CAPTURE_FOR(Defer);
        BUILD_CAPTURE_FOR(Catch);
#undef BUILD_CAPTURE_FOR
    default:
        break;
    }
    ContextBuilder::visitChildren(node, parent);

    if (kind == FunctionDecl) {
        updateFunctionDeclReturnType(node);
    }


}

template<NodeKind Kind>
VisitResult DeclarationBuilder::buildDeclaration(const ZigNode &node, const ZigNode &parent)
{
    if (shouldSkipNode(node, parent)) {
        return Recurse; // Skip this case
    }
    constexpr bool hasContext = NodeTraits::hasContext(Kind);
    constexpr bool isDef = hasContext || Kind == Module;
    bool overwrite = NodeTraits::shouldUseParentName(Kind, parent.kind());

    QString name = overwrite ? parent.spellingName() : node.spellingName();
    auto range = editorFindSpellingRange(overwrite ? parent : node, name);
    auto *decl = createDeclaration<Kind>(node, parent, name, isDef, range);
    if (Kind == Module) {
        DUChainWriteLocker lock;
        topContext()->setOwner(decl);
    }
    VisitResult ret = buildContext<Kind>(node, parent);
    if (hasContext) {
        eventuallyAssignInternalContext();
    }
    closeDeclaration();

    return ret;
}

template <NodeKind Kind>
Declaration *DeclarationBuilder::createDeclaration(const ZigNode &node, const ZigNode &parent, const QString &name, bool isDef, KDevelop::RangeInRevision &range)
{
    Identifier identifier(name);
    auto declRange = Kind == Module ? RangeInRevision::invalid(): range;
    if (Kind == Module) {
        // FIXME: Relative module name?
        //QStringList parts = session->document().str().split("/");
        //identifier = Identifier("@import " + parts.last().replace(".zig", ""));
        identifier = Identifier(session->document().str());
    }
    else if (Kind == ContainerDecl) {
        if (name.isEmpty()) {
            identifier = Identifier(node.containerName());
            declRange = node.mainTokenRange();
        }
    }
    else if (Kind == TestDecl) {
        if (name.isEmpty()) {
            // include space so it cannot be referenced as a variable
            identifier = Identifier("test 0");
        } else {
            identifier = Identifier("test " + name);
        }
    }

    DUChainWriteLocker lock;
    typename DeclType<Kind>::Type *decl = openDeclaration<typename DeclType<Kind>::Type>(
        identifier,
        declRange,
        isDef ? DeclarationIsDefinition : NoFlags
    );

    if (NodeTraits::isTypeDeclaration(Kind)) {
        decl->setKind(Declaration::Type);
    }
    lock.unlock();

    auto type = createType<Kind>(node, parent);
    openType(type);
    lock.lock();
    setDeclData<Kind>(decl);
    setType<Kind>(decl, type.data());
    lock.unlock();
    closeType();
    // {
    //     lock.lock();
    //     qDebug()  << "Create decl node:" << node.index << " name:" << identifier.toString() << " range:" << declRange << " kind:" << Kind << "type" << type->toString();
    // }
    return decl;
}

template <NodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
typename IdType<Kind>::Type::Ptr DeclarationBuilder::createType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    return typename IdType<Kind>::Type::Ptr(new typename IdType<Kind>::Type);
}

template <NodeKind Kind, EnableIf<Kind == Module || Kind == ContainerDecl>>
StructureType::Ptr DeclarationBuilder::createType(const ZigNode &node, const ZigNode &parent)
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
FunctionType::Ptr DeclarationBuilder::createType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(node);
    Q_UNUSED(parent);
    return FunctionType::Ptr(new FunctionType);
}

template <NodeKind Kind, EnableIf<!NodeTraits::isTypeDeclaration(Kind) && Kind != FunctionDecl>>
AbstractType::Ptr DeclarationBuilder::createType(const ZigNode &node, const ZigNode &parent)
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
    else if (Kind == VarDecl || Kind == FieldDecl) {
        // const bool isConst = (Kind == VarDecl) ? node.mainToken() == "const" : false;
        ZigNode typeNode = node.varType();

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
    else if (Kind ==  ParamDecl) {
        // Function arg node is the type
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(node, parent);
        return v.lastType();
    }
    else if (Kind == TestDecl) {
        return AbstractType::Ptr(new BuiltinType("test"));
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

void DeclarationBuilder::updateFunctionDeclArgs(const ZigNode &node)
{
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
            auto *param = createDeclaration<ParamDecl>(paramType, node, paramName, true, paramRange);
            {
                DUChainWriteLocker lock;
                fn->addArgument(param->abstractType(), i);
            }
            // buildContext<ParamDecl>(paramType, node);
            // eventuallyAssignInternalContext();
            closeDeclaration();

        }
    }
    DUChainWriteLocker lock;
    //fn->setReturnType(returnType);
    decl->setAbstractType(fn);
}

void DeclarationBuilder::updateFunctionDeclReturnType(const ZigNode &node)
{
    Q_ASSERT(hasCurrentDeclaration());
    auto *decl = currentDeclaration();
    auto fn = decl->type<FunctionType>();
    ZigNode typeNode = node.returnType();
    Q_ASSERT(!typeNode.isRoot());
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(typeNode, node);
    // Zig does not use error_union for inferred return types...
    auto returnType = v.lastType();

    if (auto builtin = returnType.dynamicCast<BuiltinType>()) {
        if (builtin->toString() == QLatin1String("type")) {
            NodeData data = node.data();
            ZigNode bodyNode = {node.ast, data.rhs};
            FunctionVisitor f(session, currentContext());
            f.startVisiting(bodyNode, node);
            //if (f.returnType()) {
            //    // TODO: Should wrap in some special type that tells
            //    // can be filled in later at call sites?
            returnType = f.returnType();
            //} else {
            //    qCDebug(KDEV_ZIG) << "Return type null";
            //}
        }
    }

    if (node.returnsInferredError()) {
        auto *errType = new ErrorType();
        errType->setBaseType(returnType);
        returnType = AbstractType::Ptr(errType);
    }
    DUChainWriteLocker lock;
    fn->setReturnType(returnType);
    decl->setAbstractType(fn);
    // qDebug()  << "  fn type" << fn->toString();
}

template <NodeKind Kind, EnableIf<NodeTraits::canHaveCapture(Kind)>>
void DeclarationBuilder::maybeBuildCapture(const ZigNode &node, const ZigNode &parent)
{
    QString captureName = node.captureName(CaptureType::Payload);
    if (!captureName.isEmpty()) {
        auto range = node.captureRange(CaptureType::Payload);
        // FIXME: Get type node of capture ???
        auto decl = createDeclaration<VarDecl>(node, parent, captureName, true, range);
        if (Kind == If || Kind == While) {
            // If and While captures unwrap the optional type
            ZigNode optionalType = node.nextChild();
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(optionalType, node);
            if (auto opt = v.lastType().dynamicCast<OptionalType>()) {
                DUChainWriteLocker lock;
                decl->setAbstractType(opt->baseType());
            } else if (v.lastType() != v.unknownType()) {
                // Type is known but not an optional type, this is a problem
                ProblemPointer p = ProblemPointer(new Problem());
                p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
                p->setSource(IProblem::SemanticAnalysis);
                p->setSeverity(IProblem::Hint);
                p->setDescription(i18n("Attempt to unwrap non-optional type"));
                DUChainWriteLocker lock;
                topContext()->addProblem(p);
            }
        }
        else if (Kind == For) {
            if (node.tag() == NodeTag_for_simple) {
                ZigNode sliceType = node.nextChild();
                ExpressionVisitor v(session, currentContext());
                v.startVisiting(sliceType, node);
                if (auto slice = v.lastType().dynamicCast<SliceType>()) {
                    DUChainWriteLocker lock;
                    decl->setAbstractType(slice->elementType());
                } else if (v.lastType() != v.unknownType()) {
                    // Type is known but not an optional type, this is a problem
                    ProblemPointer p = ProblemPointer(new Problem());
                    p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
                    p->setSource(IProblem::SemanticAnalysis);
                    p->setSeverity(IProblem::Hint);
                    p->setDescription(i18n("Attempt to loop non-array type"));
                    DUChainWriteLocker lock;
                    topContext()->addProblem(p);
                }
            } else {
                // TODO: Multiple capture for loop

            }

        }


        closeDeclaration();
    }
}

VisitResult DeclarationBuilder::visitUsingnamespace(const ZigNode &node, const ZigNode &parent)
{
    QString name = node.spellingName();
    auto range = editorFindSpellingRange(node, name);
    ZigNode child = node.nextChild();
    ExpressionVisitor v(session, currentContext());
    v.startVisiting(child, node);
    if (auto s = v.lastType().dynamicCast<StructureType>()) {
        DUChainWriteLocker lock;
        const auto isModule = s->modifiers() & ModuleModifier;
        const auto moduleContext = (isModule && s->declaration(nullptr)) ? s->declaration(nullptr)->topContext() : topContext();
        if (auto ctx = s->internalContext(moduleContext)) {
            currentContext()->addImportedParentContext(
                ctx,
                CursorInRevision::invalid()
            );
            return Continue;
        }
    }

    // Type is known but not an optional type, this is a problem
    ProblemPointer p = ProblemPointer(new Problem());
    p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
    p->setSource(IProblem::SemanticAnalysis);
    p->setSeverity(IProblem::Hint);
    p->setDescription(i18n("Namespace unknown or not yet resolved"));
    DUChainWriteLocker lock;
    topContext()->addProblem(p);
    return Continue;

}

} // namespace zig
