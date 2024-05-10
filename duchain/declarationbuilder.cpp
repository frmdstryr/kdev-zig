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

#include "types/builtintype.h"
#include "types/errortype.h"
#include "types/declarationtypes.h"
#include "types/optionaltype.h"
#include "types/slicetype.h"
#include "types/pointertype.h"
#include "types/comptimetype.h"
#include "types/delayedtype.h"

#include "expressionvisitor.h"
#include "functionvisitor.h"
#include "nodetraits.h"
#include "helpers.h"
#include "zigdebug.h"



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
    // qCDebug(KDEV_ZIG) << "DeclarationBuilder::visitNode" << node.index << "kind" << kind << "tag" << node.tag();
    switch (kind) {
    case VarDecl:
        return buildDeclaration<VarDecl>(node, parent);
    case FieldDecl:
        return buildDeclaration<FieldDecl>(node, parent);
    case FunctionDecl:
        return buildDeclaration<FunctionDecl>(node, parent);
    case ContainerDecl:
        return buildDeclaration<ContainerDecl>(node, parent);
    case EnumDecl:
        return buildDeclaration<EnumDecl>(node, parent);
    case UnionDecl:
        return buildDeclaration<UnionDecl>(node, parent);
    //case AliasDecl:
    //    return buildDeclaration<AliasDecl>(node, parent);
    case ErrorDecl:
        return buildDeclaration<ErrorDecl>(node, parent);
    case TestDecl:
        return buildDeclaration<TestDecl>(node, parent);
    case Module:
       return buildDeclaration<Module>(node, parent);
    case Usingnamespace:
        visitUsingnamespace(node, parent);
        break;
    case Call:
        visitCall(node, parent);
        break;
    case FnProto:
       visitFnProto(node, parent);
       break;
    default:
        break;
    }
    return ContextBuilder::visitNode(node, parent);
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
         updateFunctionArgs(node, parent);
         break;
    case ErrorDecl:
        buildErrorDecl(node, parent);
        break;
#define BUILD_CAPTURE_FOR(K) case K: maybeBuildCapture<K>(node, parent); break
        BUILD_CAPTURE_FOR(If);
        BUILD_CAPTURE_FOR(While);
        BUILD_CAPTURE_FOR(Defer);
        BUILD_CAPTURE_FOR(Catch);
#undef BUILD_CAPTURE_FOR
    case For:
        buildForCapture(node, parent);
        break;
    default:
        break;
    }
    ContextBuilder::visitChildren(node, parent);

    if (kind == FunctionDecl) {
        updateFunctionReturnType(node, parent);
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
    createDeclaration<Kind>(node, parent, name, isDef, range);
    VisitResult ret = buildContext<Kind>(node, parent);
    if (hasContext) {
        eventuallyAssignInternalContext();
    }
    // if (!Helper::isMixedType(lastType())) {
    //     session->setTypeOnNode(node, lastType());
    // }
    // session->setDeclOnNode(node, DeclarationPointer(decl));
    closeDeclaration();

    return ret;
}

template <NodeKind Kind>
Declaration *DeclarationBuilder::createDeclaration(const ZigNode &node, const ZigNode &parent, const QString &name, bool isDef, KDevelop::RangeInRevision &range)
{
    Identifier identifier(name);
    auto declRange = Kind == Module ? RangeInRevision::invalid(): range;
    if (Kind == Module) {
        QString filename = session->document().str();
        QString package = Helper::qualifierPath(filename);
        identifier = Identifier(package.isEmpty() ? filename: package);
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
            identifier = Identifier(QStringLiteral("test 0"));
        } else {
            identifier = Identifier(QStringLiteral("test %1").arg(name));
        }
    }

    if (NodeTraits::shouldSetComment(Kind)) {
        QString comment = node.comment();
        if (!comment.isEmpty()) {
            setComment(comment.toUtf8());
        }
    }

    DUChainWriteLocker lock;
    typename DeclType<Kind>::Type *decl = openDeclaration<typename DeclType<Kind>::Type>(
        identifier,
        declRange,
        isDef ? DeclarationIsDefinition : NoFlags
    );
    if (Kind == Module) {
        topContext()->setOwner(decl);
    }

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
    //     qCDebug(KDEV_ZIG)  << "Create decl node:" << node.index << "name:" << identifier.toString() << "range:" << declRange << "kind:" << Kind << "type:" << type->toString()
    //         << "context:" << (currentContext()->owner() ? currentContext()->owner()->toString() : QStringLiteral("none"));
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

template <NodeKind Kind, EnableIf<NodeTraits::isStructureDeclaration(Kind)>>
StructureType::Ptr DeclarationBuilder::createType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent)

    if (Kind == Module || Kind == ContainerDecl) {
        StructureType::Ptr structType(new StructureType);
        if (Kind == Module) {
            structType->setModifiers(ModuleModifier);
        }
        return structType;
    } else if (Kind == UnionDecl) {
        UnionType::Ptr unionType(new UnionType);
        NodeTag tag = node.tag();
        switch (tag) {
            case NodeTag_container_decl_arg: {
                ExpressionVisitor v(session, currentContext());
                v.startVisiting(node.lhsAsNode(), node);
                unionType->setBaseType(v.lastType());
                break;
            }
            case NodeTag_tagged_union:
            case NodeTag_tagged_union_trailing:
            case NodeTag_tagged_union_two:
            case NodeTag_tagged_union_two_trailing:
            case NodeTag_tagged_union_enum_tag:
            case NodeTag_tagged_union_enum_tag_trailing: {
                // Hack ?
                unionType->setBaseType(AbstractType::Ptr(new BuiltinType(QStringLiteral("enum"))));
                break;
            }
            default:
                break;
        }
        return unionType;
    }
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
    if (Kind == FieldDecl && parent.kind() == EnumDecl) {
        // Enum field
        EnumType::Ptr t(new EnumType);
        auto parentContext = session->contextFromNode(parent);
        Q_ASSERT(parentContext);
        {
            DUChainReadLocker lock;
            Q_ASSERT(parentContext->owner());
            t->setEnumType(parentContext->owner()->abstractType());
        }
        // TODO: handle rhs ? X = 1
        // Enum fields are always comptime known
        t->setComptimeKnownValue(node.mainToken());
        ZigNode rhs = node.rhsAsNode();
        if (!rhs.isRoot()) {
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(rhs, node);
            if (auto value = v.lastType().dynamicCast<BuiltinType>()) {
                if (value->isComptimeKnown()) {
                    t->setComptimeKnownValue(value->comptimeKnownValue());
                }
            }
        }
        return t;
    }
    else if (Kind == FieldDecl && node.tag() == NodeTag_error_set_decl) {
        // Error field
        EnumType::Ptr t(new EnumType);
        t->setModifiers(ErrorSetModifier);
        // Updated in buildErrorDecl
        return t;
    }
    else if (Kind == FieldDecl && parent.kind() == UnionDecl) {
        // Union field
        UnionType::Ptr u(new UnionType);
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(node.lhsAsNode(), node);
        u->setDataType(v.lastType());
        auto parentContext = session->contextFromNode(parent);
        Q_ASSERT(parentContext);
        {
            DUChainReadLocker lock;
            Q_ASSERT(parentContext->owner());
            u->setBaseType(parentContext->owner()->abstractType());
        }
        return u;
    }
    else if (Kind == FieldDecl) {
        // Struct field
        ZigNode typeNode = node.varType();
        ExpressionVisitor v(session, currentContext());
        v.setExcludedDeclaration(currentDeclaration());
        v.startVisiting(typeNode, node);
        return v.lastType();
    } else if (Kind == VarDecl) {
        const bool isConst = node.mainToken() == QLatin1String("const");
        ZigNode typeNode = node.varType();
        ZigNode valueNode = node.varValue();

        // Value only
        if (typeNode.isRoot()) {
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(valueNode, node);
            return v.lastType();
        }

        // Type and value
        ExpressionVisitor typeVisitor(session, currentContext());
        typeVisitor.startVisiting(typeNode, node);
        auto t = typeVisitor.lastType();
        // Skip for fields / var, that also requires tracking changes...
        if (!isConst || valueNode.isRoot())
            return t; // No value

        // TODO: Should it skip fields?
        // If the value is assigned try set the comptime known value
        if (auto type = dynamic_cast<ComptimeType*>(t.data())) {
            ExpressionVisitor v(session, currentContext());
            v.setInferredType(t);
            v.startVisiting(valueNode, node);

            // If we have a builtin type and a comptime known value
            // clone the type and copy the value. This is so the type
            // info not lost. Eg `const x: u8 = 1` will keep the u8 type.
            if (auto value = v.lastType().dynamicCast<BuiltinType>()) {
                if (value->isComptimeKnown()) {
                    auto comptimeType = dynamic_cast<ComptimeType*>(t->clone());
                    Q_ASSERT(comptimeType);
                    comptimeType->setComptimeKnownValue(value->comptimeKnownValue());
                    return comptimeType->asType();
                }
            }
            // If we have another comptime known value return the value
            // This may be an enum field, string or something
            // TODO: This can squash an error if the type is not correct
            else if (auto value = dynamic_cast<ComptimeType*>(v.lastType().data())) {
                if (value->isComptimeKnown()) {
                    return value->asType();
                }
            }
        }
        return t; // Has a value but could be another variable/expression, etc..
    }
    else if (Kind ==  ParamDecl) {
        // Function arg node is the type
        // The type here may be modified when updating the fn args later
        // It is also used when binding comptime types at the call site
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(node, parent);
        return v.lastType();
    }
    else if (Kind == TestDecl) {
        return AbstractType::Ptr(new BuiltinType(QStringLiteral("test")));
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
    // qCDebug(KDEV_ZIG) << "setType<Kind> IdentifiedType" << Kind;
    decl->setAlwaysForceDirect(true);
    type->setDeclaration(decl);
}

template <NodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, AbstractType *type)
{
    // (Kind == VarDecl || Kind == ParamDecl || Kind == FieldDecl)
    // qCDebug(KDEV_ZIG) << "setType<Kind> AbstractType" << Kind;
    if (Kind == FieldDecl) {
        // Set enum and union fields as a direct declarations, but not struct fields
        auto parent = decl->context()->owner();
        if (parent && (parent->type<EnumType>() || parent->type<UnionType>())) {
            if (auto enumType = dynamic_cast<EnumType*>(type)) {
                enumType->setDeclaration(decl);
                decl->setAlwaysForceDirect(true);
            }
            if (auto unionType = dynamic_cast<UnionType*>(type)) {
                unionType->setDeclaration(decl);
                decl->setAlwaysForceDirect(true);
            }
        }
    }
    decl->setAbstractType(AbstractType::Ptr(type));


}

template <NodeKind Kind>
void DeclarationBuilder::setType(Declaration *decl, StructureType *type)
{
    type->setDeclaration(decl);
    // Required for Helper::accessAttibute to find the right declaration
    decl->setAlwaysForceDirect(true);
    decl->setAbstractType(AbstractType::Ptr(type));
}

template<NodeKind Kind, EnableIf<NodeTraits::isTypeDeclaration(Kind)>>
void DeclarationBuilder::setDeclData(ClassDeclaration *decl)
{
    if (Kind == Module || Kind == ContainerDecl) {
        decl->setClassType(ClassDeclarationData::Struct);
    } else if (Kind == UnionDecl) {
        decl->setClassType(ClassDeclarationData::Union);
    }
}


template<NodeKind Kind, EnableIf<Kind == VarDecl>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    decl->setKind(Declaration::Instance);
}

// template<NodeKind Kind, EnableIf<Kind == AliasDecl>>
// void DeclarationBuilder::setDeclData(AliasDeclaration *decl)
// {
//     decl->setIsTypeAlias(true);
//     decl->setKind(Declaration::Alias);
// }

template<NodeKind Kind, EnableIf<Kind != VarDecl && Kind != Module>>
void DeclarationBuilder::setDeclData(Declaration *decl)
{
    Q_UNUSED(decl);
}

void DeclarationBuilder::updateFunctionArgs(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    Q_ASSERT(hasCurrentDeclaration());
    auto *decl = currentDeclaration();
    auto fn = decl->type<FunctionType>();
    Q_ASSERT(fn);
    const auto n = node.fnParamCount();
    for (uint32_t i=0; i < n; i++) {
        auto paramData = node.fnParamData(i);
        ZigNode paramType = {node.ast, paramData.type_expr};
        QString paramName = node.tokenSlice(paramData.name_token);
        auto paramRange = node.tokenRange(paramData.name_token);
        if (!(paramData.info.is_anytype || paramData.info.is_vararg)) {
            Q_ASSERT(!paramType.isRoot());
        }
        auto *param = createDeclaration<ParamDecl>(paramType, node, paramName, true, paramRange);
        {
            // Update the parameter type declarations
            // The type is set by the default paramter visitor (in createType)
            // but may need modified based on the parameter info (eg comptime)
            DUChainWriteLocker lock;
            if (paramData.info.is_comptime) {
                auto builtin = param->abstractType().dynamicCast<BuiltinType>();
                if (builtin && builtin->isType()) {
                    // Eg comptime T: type, create a delayed type that can
                    // be resolved at the call site
                    Zig::DelayedType::Ptr t(new Zig::DelayedType);
                    if (paramName.isEmpty()) {
                        // fn proto can skip name so just use the arg index
                        t->setIdentifier(QStringLiteral("%1").arg(i));
                    } else {
                        t->setIdentifier(paramName);
                    }
                    param->setAbstractType(t);
                } else if (param->abstractType()->modifiers() & ComptimeModifier) {
                    // No change needed
                } else {
                    // Set type to comptime
                    // TODO: is using clone needed?
                    AbstractType::Ptr comptimeType(param->abstractType()->clone());
                    comptimeType->setModifiers(comptimeType->modifiers() | ComptimeModifier);
                    param->setAbstractType(comptimeType);
                }
            }
            else if (paramData.info.is_anytype) {
                param->setAbstractType(BuiltinType::newFromName(QStringLiteral("anytype")));
            }

            fn->addArgument(param->abstractType(), i);
        }
        // buildContext<ParamDecl>(paramType, node);
        // eventuallyAssignInternalContext();
        closeDeclaration();

    }

    DUChainWriteLocker lock;
    // Set the return type to mixed, it will be updated later as it may
    // be inferred from the body
    fn->setReturnType(AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed)));
    decl->setAbstractType(fn);
}

void DeclarationBuilder::updateFunctionReturnType(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
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
        if (builtin->isType() && node.tag() == NodeTag_fn_decl) {
            // DUChainWriteLocker lock;
            FunctionVisitor f(session, currentContext());
            NodeData data = node.data();
            ZigNode bodyNode = {node.ast, data.rhs};
            f.setCurrentFunction(fn);
            f.startVisiting(bodyNode, node);
            if (Helper::isComptimeKnown(f.returnType())) {
                returnType = f.returnType();
            } else {
                auto comptimeType = f.returnType()->clone();
                comptimeType->setModifiers(comptimeType->modifiers() | ComptimeModifier);
                returnType = comptimeType;
            }
        }
    }

    if (node.returnsInferredError()) {
        ErrorType::Ptr errType(new ErrorType);
        errType->setBaseType(returnType);
        // TODO: Should it set type to anyerror?
        returnType = errType;
    }
    DUChainWriteLocker lock;
    fn->setReturnType(returnType);
    decl->setAbstractType(fn);
    // qCDebug(KDEV_ZIG)  << "  fn type" << fn->toString();
}

template <NodeKind Kind, EnableIf<NodeTraits::canHaveCapture(Kind)>>
void DeclarationBuilder::maybeBuildCapture(const ZigNode &node, const ZigNode &parent)
{
    TokenIndex tok = ast_node_capture_token(node.ast, node.index, CaptureType::Payload);
    QString captureName = node.tokenSlice(tok);
    // qCDebug(KDEV_ZIG) << "maybe build capture for" << captureName << "kind" << Kind;
    if (!captureName.isEmpty()) {
        const bool isPtr = captureName == QLatin1String("*");
        const TokenIndex nameToken = isPtr ? tok+1 : tok;
        QString name = node.tokenSlice(nameToken);
        auto range = node.tokenRange(nameToken);
        auto decl = createDeclaration<VarDecl>(node, parent, name, true, range);
        if (Kind == If || Kind == While) {
            // If and While captures unwrap the optional type
            ZigNode optionalType = node.lhsAsNode();
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(optionalType, node);
            if (auto opt = v.lastType().dynamicCast<OptionalType>()) {
                DUChainWriteLocker lock;
                decl->setAbstractType(opt->baseType());
            }
            else if (!m_prebuilding) {
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
        else if (Kind == Catch) {
            ZigNode errorType = node.lhsAsNode();
            ExpressionVisitor v(session, currentContext());
            v.startVisiting(errorType, node);
            if (auto err = v.lastType().dynamicCast<ErrorType>()) {
                DUChainWriteLocker lock;
                decl->setAbstractType(err->errorType());
            }
            else if (!m_prebuilding) {
                // Type is known but not an optional type, this is a problem
                ProblemPointer p = ProblemPointer(new Problem());
                p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
                p->setSource(IProblem::SemanticAnalysis);
                p->setSeverity(IProblem::Hint);
                p->setDescription(i18n("Attempt to catch non-error type"));
                DUChainWriteLocker lock;
                topContext()->addProblem(p);
            }
        }
        closeDeclaration();
    }

}

void DeclarationBuilder::buildForCapture(const ZigNode &node, const ZigNode &parent)
{
    Q_UNUSED(parent);
    // This is the start token of the capture eg
    // for (a, b) |*c, d| {}
    // tok would be the position of *
    TokenIndex tok = ast_node_capture_token(node.ast, node.index, CaptureType::Payload);

    const auto n = node.forInputCount();
    for (uint32_t i=0; i < n; i++) {
        // qCDebug(KDEV_ZIG) << "Create for loop capture "<< (isPtr ? "*" + name: name);
        const QString captureName = node.tokenSlice(tok);
        const bool isPtr = captureName == QLatin1String("*");
        const TokenIndex nameToken = isPtr ? tok+1 : tok;
        tok = nameToken + 2; // , next
        const QString name = node.tokenSlice(nameToken);
        Q_ASSERT(name != QLatin1String(","));
        auto range = node.tokenRange(nameToken);

        const ZigNode forInputNode = node.forInputAt(i);
        auto decl = createDeclaration<VarDecl>(forInputNode, node, name, true, range);
        ExpressionVisitor v(session, currentContext());
        v.startVisiting(forInputNode, node);
        if (auto arrayPtr = v.lastType().dynamicCast<Zig::PointerType>()) {
            // qCDebug(KDEV_ZIG) << "loop type is pointer";
            if (auto slice = arrayPtr->baseType().dynamicCast<SliceType>()) {
                DUChainWriteLocker lock;
                if (isPtr) {
                    auto ptr = new Zig::PointerType();
                    Q_ASSERT(ptr);
                    ptr->setBaseType(slice->elementType());
                    decl->setAbstractType(AbstractType::Ptr(ptr));
                } else {
                    decl->setAbstractType(slice->elementType());
                }
            }
            else if (!m_prebuilding) {
                // Type is known but not an optional type, this is a problem
                ProblemPointer p = ProblemPointer(new Problem());
                p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
                p->setSource(IProblem::SemanticAnalysis);
                p->setSeverity(IProblem::Hint);
                p->setDescription(i18n("Attempt to loop pointer of non-array type"));
                DUChainWriteLocker lock;
                topContext()->addProblem(p);
            }
        }
        else if (auto slice = v.lastType().dynamicCast<SliceType>()) {
            // qCDebug(KDEV_ZIG) << "loop type is slice";
            DUChainWriteLocker lock;
            if (isPtr) {
                auto ptr = new Zig::PointerType();
                ptr->setBaseType(slice->elementType());
                decl->setAbstractType(AbstractType::Ptr(ptr));
            } else {
                decl->setAbstractType(slice->elementType());
            }
        }
        else if (!m_prebuilding) {
            // Type is known but not an optional type, this is a problem
            // {
            //     DUChainReadLocker lock;
            //     qCDebug(KDEV_ZIG) << "for loop type is invalid " << v.lastType()->toString();
            // }
            ProblemPointer p = ProblemPointer(new Problem());
            p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
            p->setSource(IProblem::SemanticAnalysis);
            p->setSeverity(IProblem::Hint);
            if (isPtr) {
                p->setDescription(i18n("Attempt to capture pointer on non-pointer type"));
            } else {
                p->setDescription(i18n("Attempt to loop non-array type"));
            }
            DUChainWriteLocker lock;
            topContext()->addProblem(p);
        } else {
            qCDebug(KDEV_ZIG) << "for loop type is unknown";
        }
        closeDeclaration();
    }
}

void DeclarationBuilder::buildErrorDecl(const ZigNode &node, const ZigNode &parent)
{
    TokenIndex start_tok = ast_node_main_token(node.ast, node.index) + 2;
    TokenIndex end_tok = node.data().rhs;
    auto errorType = currentDeclaration()->abstractType();
    for (TokenIndex i=start_tok; i < end_tok; i++) {
        QString name = node.tokenSlice(i);
        if (name.startsWith(QLatin1Char('/')))
            continue; // Doc comment
        auto range = node.tokenRange(i);
        auto decl = createDeclaration<FieldDecl>(node, parent, name, true, range);
        auto errorValueType = decl->abstractType().staticCast<EnumType>();
        errorValueType->setEnumType(errorType);
        errorValueType->setComptimeKnownValue(name);
        DUChainWriteLocker lock;
        decl->setAbstractType(errorValueType);
        closeDeclaration();
    }
}

void DeclarationBuilder::visitFnProto(const ZigNode &node, const ZigNode &parent)
{
    if (parent.tag() != NodeTag_fn_decl) {
        auto range = node.spellingRange();
        QString name = node.fnName();
        auto decl = createDeclaration<FunctionDecl>(node, parent, name, false, range);
        {
            openContext(&node, NodeTraits::contextType(FunctionDecl), &name);
            currentContext()->setOwner(decl);
            updateFunctionArgs(node, parent);
            updateFunctionReturnType(node, parent);
            closeContext();
        }
        closeDeclaration();
    }
}

void DeclarationBuilder::visitCall(const ZigNode &node, const ZigNode &parent)
{
    if (node.isBuiltinCallTwo()
        && parent.kind() == VarDecl
        && node.mainToken() == QStringLiteral("@cImport"))
    {
        createCImportDeclaration(node, parent);
    }
    // ZigNode child = node.nextChild();
    // ExpressionVisitor v(session, currentContext());
    // v.startVisiting(child, node);
    // if (auto fn = v.lastType().dynamicCast<FunctionType>()) {
    //     // TODO: Push into helper fn
    //     AbstractType::Ptr selfType  = AbstractType::Ptr();
    //
    //     // Check if a bound fn
    //     if (child.tag() == NodeTag_field_access) {
    //         ExpressionVisitor ownerVisitor(session, currentContext());
    //         ZigNode lhs = {child.ast, child.data().lhs};
    //         ownerVisitor.startVisiting(lhs, node);
    //         auto maybeSelfType = ownerVisitor.lastType();
    //
    //         // *Self
    //         if (auto ptr = maybeSelfType.dynamicCast<PointerType>()) {
    //             maybeSelfType = ptr->baseType();
    //         }
    //
    //         // Self
    //         if (maybeSelfType.dynamicCast<StructureType>()
    //             || maybeSelfType.dynamicCast<EnumType>()
    //         ) {
    //             selfType = maybeSelfType;
    //         }
    //     }
    //
    //     // Create var declarations for parameters ?
    //     //const auto n = node.callParamCount();
    //     const auto args = fn->arguments();
    //
    //     // Handle implict "self" arg in struct fns
    //     int startArg = 0;
    //     if (args.size() > 0 && Helper::baseTypesEqual(args.at(0), selfType)) {
    //         startArg = 1;
    //     }
    //
    //     // Create bindings for delayed arguments
    //     uint32_t i = 0;
    //     for (const auto &arg: args.mid(startArg)) {
    //         if (auto delayed = arg.dynamicCast<Zig::DelayedType>()) {
    //             ZigNode argValueNode = node.callParamAt(i);
    //             if (argValueNode.isRoot()) {
    //                 continue; // Arg is missing
    //             }
    //             QString name = delayed->identifier().toString();
    //             auto range = argValueNode.range();
    //             createDeclaration<ParamDecl>(argValueNode, node, name, false, range);
    //             closeDeclaration();
    //         }
    //         i += 1;
    //     }
    //
    // }

}

Declaration* DeclarationBuilder::createCImportDeclaration(const ZigNode &node, const ZigNode &parent)
{
    Q_ASSERT(node.isBuiltinCallTwo());
    Q_ASSERT(node.mainToken() == QStringLiteral("@cImport"));
    QString name = parent.tag() == NodeTag_usingnamespace ? parent.containerName() : parent.spellingName();
    auto range = parent.spellingRange();
    auto decl = createDeclaration<ContainerDecl>(node, parent, name, true, range);
    session->setDeclOnNode(node, DeclarationPointer(decl));
    {
        openContext(&node, NodeTraits::contextType(ContainerDecl), &name);
        decl->setInternalContext(currentContext());
        ExpressionVisitor v(session, currentContext());
        decl->abstractType()->setModifiers(ModuleModifier | CIncludeModifier);
        v.setInferredType(decl->abstractType());
        v.startVisiting(node.lhsAsNode(), node);
        closeContext();
    }
    closeDeclaration();
    return decl;
}

void DeclarationBuilder::visitUsingnamespace(const ZigNode &node, const ZigNode &parent)
{
    ZigNode lhs = node.lhsAsNode();
    AbstractType::Ptr T;

    // HACK: The ExpressionVisitor for callBuiltinCImport has no
    // owner declaration so the internal context is never imported
    ExpressionVisitor v(session, currentContext());
    if (lhs.isBuiltinCallTwo() && lhs.mainToken() == QStringLiteral("@cImport")) {
        Q_ASSERT(currentContext()->owner());
        v.setInferredType(currentContext()->owner()->abstractType());
    }
    v.startVisiting(lhs, node);

    if (auto s = v.lastType().dynamicCast<StructureType>()) {
        DUChainWriteLocker lock;
        const auto isModule = s->modifiers() & ModuleModifier;
        const auto moduleContext = (isModule && s->declaration(nullptr)) ? s->declaration(nullptr)->topContext() : topContext();
        if (auto ctx = s->internalContext(moduleContext)) {
            if (parent.isRoot()) {
                topContext()->addImportedParentContext(ctx);
            } else {
                currentContext()->addImportedParentContext(ctx);
            }
            return;
        }
    }

    // Type is known but not an optional type, this is a problem
    if (!m_prebuilding) {
        ProblemPointer p = ProblemPointer(new Problem());
        auto range = node.mainTokenRange();
        p->setFinalLocation(DocumentRange(session->document(), range.castToSimpleRange()));
        p->setSource(IProblem::SemanticAnalysis);
        p->setSeverity(IProblem::Hint);
        p->setDescription(i18n("Namespace unknown or not yet resolved"));
        DUChainWriteLocker lock;
        topContext()->addProblem(p);
    }
    return;

}

} // namespace zig
