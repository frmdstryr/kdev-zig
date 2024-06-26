/*
    SPDX-FileCopyrightText: 2011-2013 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/ilanguagecontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/types/identifiedtype.h>
#include <language/backgroundparser/backgroundparser.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include <kdevelop/custom-definesandincludes/idefinesandincludesmanager.h>
#include <util/path.h>

#include <QProcess>
#include <QRegularExpression>
#include <kconfiggroup.h>

#include "types/declarationtypes.h"
#include "types/comptimetype.h"
#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/slicetype.h"

#include "helpers.h"
#include "zigdebug.h"
#include "delayedtypevisitor.h"
#include "types/enumtype.h"

namespace Zig
{

QMutex Helper::cacheMutex;
QMap<const IProject*, QVector<QUrl>> Helper::cachedCustomIncludes;
QMap<const IProject*, QVector<QUrl>> Helper::cachedSearchPaths;
QVector<QUrl> Helper::projectSearchPaths;


QMutex Helper::projectPathLock;
QMap<const KDevelop::IProject*, bool> Helper::projectPackagesLoaded;
QMap<const KDevelop::IProject*, QMap<QString, QString>> Helper::projectPackages;
QMap<const KDevelop::IProject*, int> Helper::projectTargetPointerSizes;


void Helper::scheduleDependency(
    const IndexedString& dependency,
    int betterThanPriority,
    QObject* notifyWhenReady)
{
    BackgroundParser* bgparser = KDevelop::ICore::self()->languageController()->backgroundParser();
    bool needsReschedule = true;
    if ( bgparser->isQueued(dependency) ) {
        const auto priority = bgparser->priorityForDocument(dependency);
        if ( priority > betterThanPriority - 1 ) {
            bgparser->removeDocument(dependency);
        }
        else {
            needsReschedule = false;
        }
    }
    if ( needsReschedule ) {
        // qCDebug(KDEV_ZIG) << "Rescheduled " << dependency << "at priority" << betterThanPriority;
        bgparser->addDocument(dependency, TopDUContext::ForceUpdate, betterThanPriority - 1,
                              notifyWhenReady, ParseJob::FullSequentialProcessing);
    }
}

KDevelop::IProject* Helper::findProjectWithZigPackages()
{
    for (auto project: ICore::self()->projectController()->projects()) {
        QString pkgs = project->projectConfiguration()->group(QStringLiteral("kdevzigsupport")).readEntry(QStringLiteral("zigPackages"));
        if (!pkgs.isEmpty())
            return project;
    }
    return nullptr;
}

Declaration* Helper::accessAttribute(
    const AbstractType::Ptr& accessed,
    const KDevelop::IndexedIdentifier& attribute,
    const KDevelop::TopDUContext* topContext)
{
    if ( !accessed.data() || !topContext ) {
        return nullptr;
    }

    if (auto ptr = accessed.dynamicCast<Zig::PointerType>()) {
        // Zig automatically walks pointers
        return accessAttribute(ptr->baseType(), attribute, topContext);
    }

    if (auto s = accessed.dynamicCast<StructureType>()) {
        DUChainReadLocker lock;
        // const bool isModule = s->modifiers() & ModuleModifier;
        // Context from another file?
        if (auto ctx = s->internalContext(topContext)) {
            // qCDebug(KDEV_ZIG) << "access " << attribute << "on" << s->toString() << "from" << ctx->url();
            auto decls = ctx->findDeclarations(
                attribute,
                CursorInRevision::invalid(),
                topContext,
                DUContext::DontSearchInParent
            );
            if (!decls.isEmpty()) {
                return decls.last();
            }
        }
        if (auto ctx = s->internalContext(nullptr)) {
            // qCDebug(KDEV_ZIG) << "access " << attribute << "on" << s->toString() << "from" << ctx->url();
            auto decls = ctx->findDeclarations(
                attribute,
                CursorInRevision::invalid(),
                nullptr,
                DUContext::DontSearchInParent
            );
            if (!decls.isEmpty()) {
                return decls.last();
            }
        }

    }

    // const s = enum {...}
    if (auto e = accessed.dynamicCast<EnumType>()) {
        // If accessed is an enum value, use the parent.
        if (auto enumType = e->enumType().dynamicCast<EnumType>()) {
            e = enumType;
        }
        DUChainReadLocker lock;
        if (auto ctx = e->internalContext(topContext)) {
            auto decls = ctx->findDeclarations(
                attribute, CursorInRevision::invalid(),
                topContext,
                DUContext::DontSearchInParent
            );
            if (!decls.isEmpty()) {
                return decls.first();
            }
        }
    }

    return nullptr;
}

Declaration* Helper::declarationForIdentifiedType(
        const KDevelop::AbstractType::Ptr type,
        const KDevelop::TopDUContext* topContext)
{
    DUChainReadLocker lock;
    if (auto *t = dynamic_cast<IdentifiedType*>(type.data())) {
        return t->declaration(topContext);
    }
    return nullptr;
}

bool Helper::canMergeNumericBuiltinTypes(
    const KDevelop::AbstractType::Ptr &aType,
    const KDevelop::AbstractType::Ptr &bType)
{
    if (aType->equals(bType.data()))
        return true;
    if (auto a = aType.dynamicCast<BuiltinType>()) {
        if (auto b = bType.dynamicCast<BuiltinType>()) {
            if (a->isFloat() && b->isFloat()) {
                return (a->isComptimeKnown() || b->isComptimeKnown());
            }
            if (a->isInteger() && b->isInteger()) {
                if (a->isComptimeKnown() || b->isComptimeKnown()) {
                    return true;
                }

                if ((a->isSigned() && b->isSigned())
                    || (a->isUnsigned() && b->isUnsigned())) {
                    const auto s1 = a->bitsize();
                    const auto s2 = b->bitsize();
                    return (s1 > 0 && s2 > 0 && s2 <= s1);
                }
            }
        }
    }
    return false;
}

KDevelop::AbstractType::Ptr Helper::removeComptimeValue(
        const KDevelop::AbstractType::Ptr &a)
{
    if (auto t = a.dynamicCast<EnumType>()) {
        if (t->enumType().data())
            return t->enumType(); // Use Eg if Status.Ok, return Status
        return a;
    }
    if (auto t = a.dynamicCast<UnionType>()) {
        if (t->baseType().data())
            return t->baseType(); // Use Eg if Payload.Int, return Payload
        return t;
    }

    if (auto comptimeType = dynamic_cast<const ComptimeType*>(a.data())) {
        if (comptimeType->isComptimeKnown()) {
            auto copy = dynamic_cast<ComptimeType*>(a->clone());
            Q_ASSERT(copy);
            copy->clearComptimeValue();
            return copy->asType();
        }
    }
    return a;

}

KDevelop::AbstractType::Ptr Helper::mergeTypes(
    const KDevelop::AbstractType::Ptr &a,
    const KDevelop::AbstractType::Ptr &b,
    const KDevelop::DUContext* context)
{
    Q_UNUSED(context);
    // {
    //     DUChainReadLocker lock;
    //     qCDebug(KDEV_ZIG) << "Helper::mergeTypes a=" << a->toString() << "b=" << b->toString();
    // }
    if ( a->equals(b.data()) ) {
        return a;
    }
    if (const auto t = dynamic_cast<ComptimeType*>(a.data()) ) {
        // qCDebug(KDEV_ZIG) << "two comptime types";
        if ( t->equalsIgnoringValue(b.data()) ) {
            // FIXME: This removes comptime_int/comptime_float value...
            // qCDebug(KDEV_ZIG) << "equal ignoring value";
            return removeComptimeValue(t->asType());
        }
    }

    const auto builtina = a.dynamicCast<BuiltinType>();
    if (builtina && builtina->isNull()) {
        if (auto opt = b.dynamicCast<OptionalType>()) {
            return opt;
        }
        OptionalType::Ptr r(new OptionalType);
        r->setBaseType(b);
        return r;
    }
    const auto builtinb = b.dynamicCast<BuiltinType>();
    if (builtinb && builtinb->isNull()) {
        if (auto opt = a.dynamicCast<OptionalType>()) {
            return opt;
        }
        OptionalType::Ptr r(new OptionalType);
        r->setBaseType(a);
        return r;
    }

    if (builtina && builtinb) {
        // If both are floats and one is comptime use the non-comptime one
        if (builtina->isFloat() && builtinb->isFloat()) {
            return builtina->isComptimeKnown() ? b : a;
        }
        // If both are ints and one is comptime use the non-comptime one
        if (builtina->isInteger() && builtinb->isInteger()) {
            return builtina->isComptimeKnown() ? b : a;
        }

    }

    if (const auto opt = a.dynamicCast<OptionalType>()) {
        // TODO: Promotion ?
        if (opt->baseType()->equals(b.data())
            || canMergeNumericBuiltinTypes(opt->baseType(), b)
        ) {
            return opt;
        }
    }
    if (const auto opt = b.dynamicCast<OptionalType>()) {
        // TODO: Builtin promotion ?
        if (opt->baseType()->equals(a.data())
            || canMergeNumericBuiltinTypes(opt->baseType(), a)
        ) {
            return opt;
        }
    }

    // Two strings with different dimensions (eg `if (x) "true" else "false"`)
    // merge into a type with no size like *const [:0]u8
    if (const auto aptr = a.dynamicCast<PointerType>()) {
        const auto bptr = b.dynamicCast<PointerType>();
        if (bptr && aptr->modifiers() == bptr->modifiers()) {
            const auto sa = aptr->baseType().dynamicCast<SliceType>();
            const auto sb = bptr->baseType().dynamicCast<SliceType>();
            if (sa && sa->equalsIgnoringValueAndDimension(sb.data())) {
                PointerType::Ptr ptr(new PointerType);
                SliceType::Ptr slice(new SliceType);
                slice->setElementType(sa->elementType());
                slice->setModifiers(sa->modifiers());
                slice->setSentinel(sa->sentinel());
                ptr->setBaseType(slice);
                ptr->setModifiers(aptr->modifiers());
                return ptr;
            }
        }
    }

    // Else, idk how to merge so its mixed
    return AbstractType::Ptr(new IntegralType(IntegralType::TypeMixed));
}

static inline bool contextTypeIsFnOrClass(const DUContext* ctx)
{
    return (
        ctx->type() == DUContext::Function
        || ctx->type() == DUContext::Class // Also needed for Zig's nested structs
        || ctx->type() == DUContext::Global
    );
}

/**
 * In Zig order does not matter in container bodies
 */
static inline bool canFindBeyondUse(const DUContext* ctx)
{
    return (
        ctx && ctx->owner() && (
             ctx->owner()->isFunctionDeclaration()
             || (ctx->owner()->kind() == Declaration::Kind::Type)
             || (ctx->owner()->kind() == Declaration::Kind::Instance && (
                 ctx->parentContext() && ctx->parentContext()->owner() &&
                 ctx->parentContext()->owner()->kind() == Declaration::Kind::Type
             ))
        )
    );
}

Declaration* Helper::declarationForName(
    const QString& name,
    const CursorInRevision& location,
    DUChainPointer<const DUContext> context,
    const KDevelop::Declaration* excludedDeclaration)
{
    DUChainReadLocker lock;
    const DUContext* currentContext = context.data();
    bool findBeyondUse = canFindBeyondUse(currentContext);
    //qCDebug(KDEV_ZIG) << "Find" << name << " beyond use" << findBeyondUse;
    CursorInRevision findUntil = findBeyondUse ? currentContext->topContext()->range().end : location;
    auto identifier = KDevelop::Identifier(name);
    // for (Declaration* declaration: context->localDeclarations()) {
    //     qCDebug(KDEV_ZIG) << "local decls " << declaration->toString();
    // }
    auto localDeclarations = context->findLocalDeclarations(
        identifier,
        findUntil,
        nullptr,
        AbstractType::Ptr(), DUContext::DontResolveAliases);
    for (Declaration* declaration: localDeclarations) {
        if (declaration != excludedDeclaration)
            return declaration;
    }

    QList<Declaration*> declarations;
    do {
        findUntil = findBeyondUse ? currentContext->topContext()->range().end : location;
        declarations = currentContext->findDeclarations(identifier, findUntil);

        for (Declaration* declaration: declarations) {
            //qCDebug(KDEV_ZIG) << "decl " << declaration->toString();
            if (declaration->context()->type() != DUContext::Class
                || (
                    contextTypeIsFnOrClass(currentContext)
                    //&& declaration->context() == currentContext->parentContext()
                )
            ) {
                    // Declarations from struct decls must be referenced through `self.<foo>`, except
                    //  in their local scope (handled above) or when used as default arguments for methods of the same class.
                    // Otherwise, we're done!
                if (declaration != excludedDeclaration)
                    return declaration;
            }
        }
        if (!declarations.isEmpty()) {
            // If we found declarations but rejected all of them (i.e. didn't return), we need to keep searching.
            declarations.clear();
        }

        if (!findBeyondUse && canFindBeyondUse(currentContext)) {
            // Names in the body may be defined after the function definition, before the function is called.
            // Note: only the parameter list has type DUContext::Function, so we have to do this instead.
            findBeyondUse = true;
        }
    } while ((currentContext = currentContext->parentContext()));

    return nullptr;
}

KDevelop::DUContext* Helper::thisContext(
        const KDevelop::CursorInRevision& location,
        const KDevelop::TopDUContext* topContext)
{
    if (!location.isValid()) {
        return nullptr;
    }
    DUContext* currentContext = topContext->findContextAt(location);
    while (currentContext) {
        auto contextType = currentContext->type();
        if (contextType == DUContext::Class
            || contextType == DUContext::Namespace
            || contextType == DUContext::Enum
            || contextType == DUContext::Global
        ) {
            return currentContext;
        }
        currentContext = currentContext->parentContext();
    }
    return nullptr;

}

const KDevelop::AbstractType::Ptr Helper::unwrapPointer(const KDevelop::AbstractType::Ptr &type)
{
    if (auto ptr = type.dynamicCast<Zig::PointerType>())
        return ptr->baseType();
    return type;
}

bool Helper::baseTypesEqual(
    const KDevelop::AbstractType::Ptr &a,
    const KDevelop::AbstractType::Ptr &b)
{
    if (!a || !b)
        return false;
    return typesEqualIgnoringModifiers(Helper::unwrapPointer(a), Helper::unwrapPointer(b));
}

bool Helper::typesEqualIgnoringComptimeValue(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b)
{
    if (!a || !b)
        return false;
    if (auto A = dynamic_cast<const ComptimeType*>(a.data()))
        return A->equalsIgnoringValue(b.data());
    if (auto B = dynamic_cast<const ComptimeType*>(b.data()))
        return B->equalsIgnoringValue(a.data());
    return a->equals(b.data());
}

bool Helper::typesEqualIgnoringModifiers(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b)
{
    if (!a || !b)
        return false;
    if (a->equals(b.data()))
        return true;
    auto copy = b->clone();
    copy->setModifiers(a->modifiers());
    bool result = a->equals(copy);
    if (!result) {
        auto comptimeType = dynamic_cast<const ComptimeType*>(a.data());
        result = comptimeType && comptimeType->equalsIgnoringValue(copy);
    }
    delete copy;
    return result;
}

bool Helper::isComptimeKnown(const KDevelop::AbstractType::Ptr &a)
{
    if (!a.data())
        return false;
    if (a->modifiers() & ComptimeModifier)
        return true;
    if (auto comptimeType = dynamic_cast<const ComptimeType*>(a.data()))
        return comptimeType->isComptimeKnown();
    return false;
}

bool Helper::canTypeBeAssigned(
        const KDevelop::AbstractType::Ptr &targetType,
        const KDevelop::AbstractType::Ptr &valueType,
        const KDevelop::IProject* project)
{
    // Convert c-types
    auto target = Helper::asZigType(targetType);
    auto value = Helper::asZigType(valueType);
    // TODO: Handled delayed types
    // {
    //     DUChainReadLocker lock;
    //     qCDebug(KDEV_ZIG) << "Check if "<< value->toString()
    //         << "can be assigned to " << target->toString();
    // }

    // undefined can be assigned to anything except a type
    if (const auto v = value.dynamicCast<BuiltinType>()) {
        if (v->isUndefined()) {
            if (const auto t = target.dynamicCast<BuiltinType>())
                return !(t->isType() || t->isAnytype());
            return true;
        }
    }

    // Handles the rest
    if (const auto t = dynamic_cast<const ComptimeType*>(target.data()))
        return t->canValueBeAssigned(value, project);

    // // Const param/capture can be assigned to non-const
    // if (auto enumeration = target.dynamicCast<EnumerationType>()) {
    //     return typesEqualIgnoringModifiers(enumeration, value);
    // }

    return target->equals(value.data());
}


bool Helper::isMixedType(const KDevelop::AbstractType::Ptr &a, bool checkPtr)
{
    if (checkPtr) {
        if (auto ptr = a.dynamicCast<PointerType>())
            return isMixedType(ptr->baseType(), false);
    }
    if (auto it = a.dynamicCast<BuiltinType>())
        return it->isAnytype();
    if (auto it = a.dynamicCast<IntegralType>())
        return it->dataType() == IntegralType::TypeMixed;
    return false;
}

AbstractType::Ptr Helper::asZigType(const AbstractType::Ptr &a)
{
    if (auto it = a.dynamicCast<KDevelop::PointerType>()) {
        Zig::PointerType::Ptr ptr(new Zig::PointerType);
        ptr->setModifiers(it->modifiers());
        ptr->setBaseType(Helper::asZigType(it->baseType()));
        return ptr;
    }
    else if (auto it = a.dynamicCast<KDevelop::ArrayType>()) {
        if (it->dimension() > 0) {
            Zig::SliceType::Ptr slice(new Zig::SliceType);
            slice->setModifiers(it->modifiers());
            slice->setDimension(it->dimension());
            slice->setElementType(Helper::asZigType(it->elementType()));
            return slice;
        } else {
            Zig::PointerType::Ptr ptr(new Zig::PointerType);
            ptr->setModifiers(it->modifiers());
            ptr->setBaseType(Helper::asZigType(it->elementType()));
            return ptr;
        }

    }
    else if (auto it = a.dynamicCast<IntegralType>()) {
        switch (it->dataType()) {
            case IntegralType::TypeVoid:
                return BuiltinType::newFromName(QStringLiteral("void"));
            case IntegralType::TypeNull:
                return BuiltinType::newFromName(QStringLiteral("null"));
            case IntegralType::TypeChar:
                return BuiltinType::newFromName(
                   (it->modifiers() & AbstractType::UnsignedModifier) ?
                   QStringLiteral("u8") : QStringLiteral("i8")
                );
            case IntegralType::TypeBoolean:
                return BuiltinType::newFromName(QStringLiteral("bool"));
            case IntegralType::TypeByte:
                return BuiltinType::newFromName(QStringLiteral("u8"));
            case IntegralType::TypeSbyte:
                return BuiltinType::newFromName(QStringLiteral("i8"));
            case IntegralType::TypeShort:
                return BuiltinType::newFromName(
                    (it->modifiers() & AbstractType::UnsignedModifier) ?
                    QStringLiteral("c_ushort") : QStringLiteral("c_short"));
            case IntegralType::TypeInt:
                return BuiltinType::newFromName(
                    (it->modifiers() & AbstractType::UnsignedModifier) ?
                    QStringLiteral("c_uint") : QStringLiteral("c_int"));
            case IntegralType::TypeLong:
                if (it->modifiers() & AbstractType::LongLongModifier) {
                    return BuiltinType::newFromName(
                        (it->modifiers() & AbstractType::UnsignedModifier) ?
                        QStringLiteral("c_ulonglong") : QStringLiteral("c_longlong"));
                } else {
                    return BuiltinType::newFromName(
                        (it->modifiers() & AbstractType::UnsignedModifier) ?
                        QStringLiteral("c_ulong") : QStringLiteral("c_long"));
                }
            case IntegralType::TypeFloat:
                return BuiltinType::newFromName(QStringLiteral("f32"));
            case IntegralType::TypeDouble:
                return BuiltinType::newFromName(QStringLiteral("f64"));
            default:
                break;
        }
    }


    return a;
}


AbstractType::Ptr Helper::evaluateUnsignedOp(
    const BuiltinType::Ptr &a,
    const BuiltinType::Ptr &b,
    const NodeTag &tag)
{
    Q_ASSERT(a->isUnsigned() && b->isUnsigned());
    Q_ASSERT(a->isComptimeKnown() && b->isComptimeKnown());
    bool ok1 = false;
    bool ok2 = false;
    bool ok3 = true;
    QString va = a->comptimeKnownValue().str();
    QString vb = b->comptimeKnownValue().str();
    const uint64_t v1 = va.toULongLong(&ok1, 0);
    const uint64_t v2 = vb.toULongLong(&ok2, 0);
    if (ok1 && ok2) {
        uint64_t result = 0;
        // check overflow ???
        if (tag == NodeTag_add)
            result = v1 + v2;
        else if (tag == NodeTag_sub)
            result = v1 - v2;
        else if (tag == NodeTag_shl)
            result = v1 << v2;
        else if (tag == NodeTag_shr)
            result = v1 >> v2;
        else if (tag == NodeTag_bit_or)
            result = v1 | v2;
        else if (tag == NodeTag_bit_and)
            result = v1 & v2;
        else
            ok3 = false;
        if (ok3) {
            // TODO: Resolve the type? check overflow ???
            auto sizea = a->bitsize();
            auto sizeb = b->bitsize();
            auto r = static_cast<BuiltinType*>((sizea >= sizeb) ? a->clone() : b->clone());
            const int base = (va.startsWith(QStringLiteral("0x")) || vb.startsWith(QStringLiteral("0x"))) ? 16 : 10;
            QString prefix = (base == 16 || result > 255 ) ? QStringLiteral("0x") : QStringLiteral("");
            r->setComptimeKnownValue(QStringLiteral("%1%2").arg(prefix).arg(result, 0, base));
            return AbstractType::Ptr(r);
        }
    }

    // IDK how to add so make the type longer comptime
    auto r = static_cast<BuiltinType*>(a->clone());
    r->clearComptimeValue();
    return AbstractType::Ptr(r);
}

KDevelop::Declaration* Helper::declarationForImportedModuleName(
        const QString& module, const QString& currentFile)
{
    QStringList parts = module.split(QStringLiteral("."));
    if (parts.isEmpty()) {
        return nullptr;
    }
    QUrl package = importPath(parts.at(0), currentFile);
    if (package.isEmpty()) {
        qCDebug(KDEV_ZIG) << "imported module does not exist" << module;
        return nullptr;
    }

    DUChainReadLocker lock;
    auto *mod = DUChain::self()->chainForDocument(package);

    // if (!mod && waitForUpdate) {
    //     // Module not yet parsed, reschedule with a very high priority
    //     // and wait for it to update
    //     qCDebug(KDEV_ZIG) << "waiting for " << package << " to parse...";
    //     lock.unlock();
    //     IndexedString doc(package);
    //     const int maxPrio = std::numeric_limits<int>::max() - 0x100;
    //     Helper::scheduleDependency(doc, maxPrio);
    //     mod = DUChain::self()->waitForUpdate(
    //         doc, KDevelop::TopDUContext::AllDeclarationsAndContexts).data();
    //     lock.lock();
    // }
    if (!mod || !mod->owner()) {
        qCDebug(KDEV_ZIG) << "imported module is invalid" << module;
        return nullptr;
    }
    Declaration* decl = mod->owner();
    lock.unlock();
    for (const auto &part: parts.mid(1)) {
        if (part.isEmpty()) {
            qCDebug(KDEV_ZIG) << "cant import module with empty part " << module;
            return nullptr;
        }
        decl = Helper::accessAttribute(decl->abstractType(), part, decl->topContext());
        if (!decl) {
            qCDebug(KDEV_ZIG) << "no decl for" << part << "of" << module;
            return nullptr;
        }

        // If the decl is a delayed import that is not yet resolved
        // Reschedule the delayed import at a high priority and
        // wait for it to update.
        // TODO: This will still not work for exprs like @import("foo").bar
        // auto delayedImport = decl->abstractType().dynamicCast<DelayedType>();
        // if (waitForUpdate && delayedImport && (delayedImport->modifiers() & ModuleModifier)) {
        //     IndexedString doc(delayedImport->identifier());
        //     qCDebug(KDEV_ZIG) << "waiting for delayed import " << doc.str() << " to parse...";
        //     // Reschedule delayed import at high priority
        //     const int maxPrio = std::numeric_limits<int>::max() - 0x100;
        //     Helper::scheduleDependency(doc, maxPrio);
        //     mod = DUChain::self()->waitForUpdate(
        //         doc,
        //         KDevelop::TopDUContext::AllDeclarationsAndContexts).data();
        //     lock.lock();
        //     if (!mod || !mod->owner()) {
        //         qCDebug(KDEV_ZIG) << "delayed import " << doc.str() << "is empty...";
        //         return nullptr;
        //     }
        //     decl = mod->owner();
        //     lock.unlock();
        // }
    }
    return decl;
}

QString Helper::zigExecutablePath(const IProject* project)
{
    if ( project ) {
        auto zigExe = project->projectConfiguration()->group(QStringLiteral("kdevzigsupport")).readEntry(QStringLiteral("zigExecutable"));
        if ( !zigExe.isEmpty() ) {
            QFile f(zigExe);
            if ( f.exists() ) {
                return zigExe;
            }
        }
    }
    auto result = QStandardPaths::findExecutable(QStringLiteral("zig"));
    if ( ! result.isEmpty() ) {
        return result;
    }
    qCWarning(KDEV_ZIG) << "zig exe not found. Using default";
    return QStringLiteral("/usr/bin/zig");
}

void Helper::loadPackages(const KDevelop::IProject* project)
{
    if (!project) {
        return;
    }
    auto &projectSpecificPackages = projectPackages[project];

    QMutexLocker lock(&Helper::projectPathLock);

    // Keep old stdlib to avoid re-running zig
    QString oldStdLib = projectSpecificPackages.contains(QStringLiteral("std")) ?
        *projectSpecificPackages.constFind(QStringLiteral("std")): QStringLiteral("");

    projectSpecificPackages.clear();

    // Load packages

    QString pkgs = project->projectConfiguration()->group(QStringLiteral("kdevzigsupport")).readEntry(QStringLiteral("zigPackages"));
    qCDebug(KDEV_ZIG) << "zig packages configured" << pkgs;
    for (const QString &pkg : pkgs.split(QStringLiteral("\n"))) {
        QStringList parts = pkg.split(QStringLiteral(":"));
        if (parts.size() == 2) {
            QString pkgName = parts[0].trimmed();
            QString pkgPath = parts[1].trimmed();
            if (!pkgName.isEmpty() && !pkgPath.isEmpty()) {
                QString finalPath = QDir::isAbsolutePath(pkgPath) ? pkgPath :
                    QDir(project->path().toLocalFile()).absoluteFilePath(pkgPath);
                qCDebug(KDEV_ZIG) << "zig package set: " << pkgName << ": " << finalPath;
                projectSpecificPackages.insert(pkgName, finalPath);
            }
        } else {
            qCDebug(KDEV_ZIG) << "zig package is invalid format: " << pkg;
        }
    }

    // If none is defined use the last saved
    if (!oldStdLib.isEmpty() && !projectSpecificPackages.contains(QStringLiteral("std"))) {
        projectSpecificPackages.insert(QStringLiteral("std"), oldStdLib);
    }

    projectPackagesLoaded.insert(project, true);
}

int Helper::targetPointerBitsize(const KDevelop::IProject* project)
{
    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectTargetPointerSizes.contains(project)) {
        int targetSize = -1;
        if (project) {
            targetSize = project->projectConfiguration()->group(QStringLiteral("kdevzigsupport")).readEntry(QStringLiteral("zigTargetPtrSize"), 0);
        } else {
            // Find first defined zigTargetPointerSize
            for (auto p: ICore::self()->projectController()->projects()) {
                int s = p->projectConfiguration()->group(QStringLiteral("kdevzigsupport")).readEntry(QStringLiteral("zigTargetPtrSize"), 0);
                if (s > 0) {
                    targetSize = s;
                    break;
                }
            }
        }
        if (targetSize < 1)
            targetSize = -1;
        projectTargetPointerSizes[project] = targetSize * 8;
    }
    return projectTargetPointerSizes[project];
}

QString Helper::stdLibPath(const IProject* project)
{
    static QRegularExpression stdDirPattern(QStringLiteral("\\s*\"std_dir\":\\s*\"(.+)\""));
    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded[project]) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }
    // Use one from project config or previously loaded from zig
    if (projectPackages[project].contains(QStringLiteral("std"))) {
        QDir stdzig = *projectPackages[project].constFind(QStringLiteral("std"));
        stdzig.cdUp();
        return stdzig.path();
    }
    QProcess zig;
    QStringList args = {QStringLiteral("env")};
    QString zigExe = zigExecutablePath(project);
    qCDebug(KDEV_ZIG) << "zig exe" << zigExe;
    if (QFile(zigExe).exists()) {
        zig.start(zigExe, args);
        zig.waitForFinished(1000);
        QString output = QString::fromUtf8(zig.readAllStandardOutput());
        qCDebug(KDEV_ZIG) << "zig env output:" << output;
        QStringList lines = output.split(QStringLiteral("\n"));
        for (const QString &line: lines) {
            auto r = stdDirPattern.match(line);
            if (r.hasMatch()) {
                QString match = r.captured(1);
                QString path = QDir::isAbsolutePath(match) ? match :
                    QDir::home().filePath(match);
                qCDebug(KDEV_ZIG) << "std_lib" << path;
                projectPackages[project].insert(QStringLiteral("std"), QDir::cleanPath(QStringLiteral("%1/std.zig").arg(path)));
                QDir stdzig = *projectPackages[project].constFind(QStringLiteral("std"));
                stdzig.cdUp();
                return stdzig.path();
            }
        }
    }
    qCWarning(KDEV_ZIG) << "zig std lib path not found";
    return QStringLiteral("/usr/local/lib/zig/lib/zig/std");
}

QString Helper::packagePath(const QString &name, const QString& currentFile)
{
    auto project = ICore::self()->projectController()->findProjectForUrl(QUrl::fromLocalFile(currentFile));
    if (!project)
        project = findProjectWithZigPackages();

    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded[project]) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }
    if (projectPackages[project].contains(name)) {
        return *projectPackages[project].constFind(name);
    }
    if (name == QLatin1String("std")) {
        lock.unlock();
        return QDir::cleanPath(QStringLiteral("%1/std.zig").arg(stdLibPath(project)));
    }
    qCDebug(KDEV_ZIG) << "No zig package path found for " << name;
    return QStringLiteral("");
}

QUrl Helper::importPath(const QString& importName, const QString& currentFile)
{
    QUrl importPath;
    if (importName.endsWith(QStringLiteral(".zig"))) {
        QDir folder = currentFile;
        folder.cdUp();
        importPath = QUrl::fromLocalFile(
            QDir::cleanPath(folder.absoluteFilePath(importName))
        );
    } else {
        importPath = QUrl::fromLocalFile(packagePath(importName, currentFile));
    }
    if (QFile(importPath.toLocalFile()).exists()) {
        return importPath;
    }
    qCDebug(KDEV_ZIG) << "@import(" << importName << ") does not exist" << importPath.toLocalFile();
    return QUrl(QStringLiteral(""));
}

QString Helper::packageName(const QString &currentFile)
{
    auto project = ICore::self()->projectController()->findProjectForUrl(QUrl::fromLocalFile(currentFile));
    if (!project)
        project = findProjectWithZigPackages();
    // Even if there is no project it should still find the std import
    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded[project]) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }
    for (const auto& pkgName: projectPackages[project].keys()) {
        const auto& pkgPath = projectPackages[project][pkgName];
        if (pkgPath.isEmpty() || pkgName.isEmpty())
            continue;
        if (currentFile == pkgPath)
            return pkgName;
    }
    return QStringLiteral("");
}

QString Helper::qualifierPath(const QString& currentFile)
{
    QString f = currentFile.endsWith(QStringLiteral(".zig")) ? currentFile.mid(0, currentFile.size()-3) : currentFile;
    auto project = ICore::self()->projectController()->findProjectForUrl(QUrl::fromLocalFile(currentFile));
    if (!project)
        project = findProjectWithZigPackages();
    // Even if there is no project it should still find the std import
    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded[project]) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }

    for (const auto& pkgName: projectPackages[project].keys()) {
        const auto& pkgPath = projectPackages[project][pkgName];
        if (pkgPath.isEmpty() || pkgName.isEmpty())
            continue;
        if (currentFile == pkgPath)
            return pkgName;
        QFileInfo pkg(pkgPath);
        QDir pkgRoot(pkg.absolutePath());
        QString relPath = pkgRoot.relativeFilePath(f);
        if (relPath.isEmpty() || relPath == QLatin1Char('.'))
            return pkgName;
        if (!relPath.startsWith(QStringLiteral(".."))) {
            QString subPkg = relPath.replace(QDir::separator(), QLatin1Char('.'));
            if (subPkg.endsWith(QLatin1Char('.')))
                subPkg = subPkg.mid(0, subPkg.size()-1);
            return QStringLiteral("%1.%2").arg(pkgName, subPkg);
        }
    }
    if (project) {
        QDir projectRoot(project->path().toLocalFile());
        QString relPath = projectRoot.relativeFilePath(f);
        if (!relPath.startsWith(QStringLiteral("..")))
            return relPath.replace(QDir::separator(), QLatin1Char('.'));
    }
    return QStringLiteral(""); // f.mid(1).replace(QDir::separator(), '.');
}

QUrl Helper::includePath(const QString &name, const QString& currentFile)
{
    // Look for relative include
    if ( QDir::isAbsolutePath(name) )
        return QUrl(name);
    auto localPath = QDir(currentFile).filePath(name);
    if ( QFile::exists(localPath) )
        return QUrl(localPath);

    // TODO: Get standard paths?
    auto project = ICore::self()->projectController()->findProjectForUrl(QUrl::fromLocalFile(currentFile));
    if (project) {
        auto buildManager = project->buildSystemManager();
        if (buildManager) {
            auto items = project->itemsForPath(IndexedString(currentFile));
            if (!items.isEmpty()) {
                for (const auto& includeDir: buildManager->includeDirectories(items.first())) {
                    auto relativePath = QDir(includeDir.toLocalFile()).filePath(name);
                    if ( QFile::exists(relativePath) )
                        return QUrl(relativePath);
                }
            }
        }
        for (const auto& includeDir: IDefinesAndIncludesManager::manager()->includes(name))
        {
            auto relativePath = QDir(includeDir.toLocalFile()).filePath(name);
            if ( QFile::exists(relativePath) )
                return QUrl(relativePath);
        }
    }
    // Give up, just return missing file
    return QUrl(name);
}

void ScheduleDependency::updateReady(const IndexedString& url, const ReferencedTopDUContext& topContext)
{
    Q_UNUSED(url);
    Q_UNUSED(topContext);
    // Reparse the original document again
    Helper::scheduleDependency(m_documentUrl, 0);
    this->deleteLater();
}

} // namespace zig
