/*
    SPDX-FileCopyrightText: 2011-2013 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "helpers.h"
#include <language/duchain/duchainlock.h>
#include "types/declarationtypes.h"
#include "types/pointertype.h"

namespace Zig
{


Declaration* Helper::accessAttribute(const AbstractType::Ptr accessed,
                                     const KDevelop::IndexedIdentifier& attribute,
                                     const KDevelop::TopDUContext* topContext)
{
    if ( ! accessed || !topContext ) {
        return nullptr;
    }

    if (auto ptr = accessed.dynamicCast<Zig::PointerType>()) {
        // Zig automatically walks pointers
        return accessAttribute(ptr->baseType(), attribute, topContext);
    }

    if (auto s = accessed.dynamicCast<StructureType>()) {
        if (auto ctx = s->internalContext(topContext)) {
            auto decls = ctx->findDeclarations(
                attribute, CursorInRevision::invalid(),
                topContext, DUContext::DontSearchInParent);
            if (!decls.isEmpty()) {
                return decls.first();
            }
        }
    }

    return nullptr;
}


Declaration* Helper::declarationForName(
    const QString& name,
    const CursorInRevision& location,
    DUChainPointer<const DUContext> context)
{
    DUChainReadLocker lock;
    auto identifier = KDevelop::Identifier(name);
    auto localDeclarations = context->findLocalDeclarations(identifier, location, nullptr,
                                                            AbstractType::Ptr(), DUContext::DontResolveAliases);
    if ( !localDeclarations.isEmpty() ) {
        return localDeclarations.last();
    }

    QList<Declaration*> declarations;
    const DUContext* currentContext = context.data();
    bool findInNext = true, findBeyondUse = false;
    do {
        if (findInNext) {
            CursorInRevision findUntil = findBeyondUse ? currentContext->topContext()->range().end : location;
            declarations = currentContext->findDeclarations(identifier, findUntil);

            for (Declaration* declaration: declarations) {
                if (declaration->context()->type() != DUContext::Class ||
                    (currentContext->type() == DUContext::Function && declaration->context() == currentContext->parentContext())) {
                     // Declarations from class decls must be referenced through `self.<foo>`, except
                     //  in their local scope (handled above) or when used as default arguments for methods of the same class.
                     // Otherwise, we're done!
                    return declaration;
                }
            }
            if (!declarations.isEmpty()) {
                // If we found declarations but rejected all of them (i.e. didn't return), we need to keep searching.
                findInNext = true;
                declarations.clear();
            }
        }

        if (!findBeyondUse && currentContext->owner() && currentContext->owner()->isFunctionDeclaration()) {
            // Names in the body may be defined after the function definition, before the function is called.
            // Note: only the parameter list has type DUContext::Function, so we have to do this instead.
            findBeyondUse = findInNext = true;
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
        ) {
            return currentContext;
        }
        currentContext = currentContext->parentContext();
    }
    return nullptr;

}


} // namespace zig
