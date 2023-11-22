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
#include <language/duchain/ducontext.h>
#include <language/duchain/declaration.h>
#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>



#include "duchain/zignode.h"
#include "duchain/helpers.h"

#include "item.h"
#include "context.h"
#include "zigdebug.h"

namespace Zig
{

using namespace KDevelop;

CompletionContext::CompletionContext(DUContextPointer context,
                                     const QString &contextText,
                                     const QString &followingText,
                                     const CursorInRevision &position,
                                     int depth)
    : KDevelop::CodeCompletionContext(context, contextText, position, depth),
      m_followingText(followingText)
{

}

QList<CompletionTreeItemPointer> CompletionContext::completionItems(bool &abort, bool fullCompletion)
{
    QList<CompletionTreeItemPointer> items;

    if (!m_duContext) {
        return items;
    }
    DUChainReadLocker lock(DUChain::lock(), 100);
    if (!lock.locked()) {
        return items;
    }
    auto top = m_duContext->topContext();
    auto localContext = top->findContextAt(m_position);
    if (!localContext)
        return items; // Can it be null ?
    ZigCompletion completion(complete_expr(m_text.toUtf8(), m_followingText.toUtf8()));
    if (!completion.data())
        return items;
    CompletionResultType result_type = completion.data()->result_type;
    if (result_type == CompletionField) {
        QString name = completion.data()->name;
        qCDebug(KDEV_ZIG) << "Field completion on: " << name;
        Declaration* decl = Helper::declarationForName(
            name,
            m_position,
            DUChainPointer<const DUContext>(localContext));
        if (!decl)
            return items;
        if (const auto t = decl->abstractType().dynamicCast<EnumType>())
            items.append(completionsForEnum(t));
        else if (const auto t = decl->abstractType().dynamicCast<UnionType>())
            items.append(completionsForUnion(t));
        else if (const auto t = decl->abstractType().dynamicCast<StructureType>())
            items.append(completionsForStruct(t));
        else if (const auto t = decl->abstractType().dynamicCast<SliceType>())
            items.append(completionsForSlice(t));
        // else what is it ?
    }
    else {
        for(const auto &it : top->allDeclarations(CursorInRevision::invalid(), top))
        {
            const auto decl = it.first;
            if(!decl
                || decl->identifier() == globalImportIdentifier()
                || decl->identifier() == globalAliasIdentifier()
                || decl->identifier() == Identifier())
                continue;
            items << CompletionTreeItemPointer(new CompletionItem(DeclarationPointer(decl)));
        }
    }
    return items;
}

QList<KDevelop::CompletionTreeItemPointer> CompletionContext::completionsForStruct(const StructureType::Ptr &t) const
{
    QList<CompletionTreeItemPointer> items;
    const auto *top = m_duContext->topContext();
    if (const auto ctx = t->internalContext(top)) {
        return completionsFromLocalDecls(DUContextPointer(ctx));
    }
    return items;
}

QList<KDevelop::CompletionTreeItemPointer> CompletionContext::completionsForEnum(const EnumType::Ptr &t) const
{
    if (auto baseType = t->enumType().dynamicCast<EnumType>()) {
        return completionsForEnum(baseType);
    }
    const auto *top = m_duContext->topContext();
    if (const auto ctx = t->internalContext(top)) {
        return completionsFromLocalDecls(DUContextPointer(ctx));
    }
    return {};
}

QList<KDevelop::CompletionTreeItemPointer> CompletionContext::completionsForUnion(const UnionType::Ptr &t) const
{
    if (auto baseType = t->baseType().dynamicCast<UnionType>()) {
        return completionsForUnion(baseType);
    }
    const auto *top = m_duContext->topContext();
    if (const auto ctx = t->internalContext(top)) {
        return completionsFromLocalDecls(DUContextPointer(ctx));
    }
    return {};
}

QList<KDevelop::CompletionTreeItemPointer> CompletionContext::completionsForSlice(const SliceType::Ptr &t) const
{
    // TODO: .len .ptr?
    return {};
}

QList<KDevelop::CompletionTreeItemPointer> CompletionContext::completionsFromLocalDecls(const DUContextPointer &ctx) const
{
    QList<CompletionTreeItemPointer> items;
    const auto *top = m_duContext->topContext();
    for(auto *d : ctx->localDeclarations(top))
    {
        if(!d
            || d->identifier() == globalImportIdentifier()
            || d->identifier() == globalAliasIdentifier()
            || d->identifier() == Identifier())
            continue;
        items << CompletionTreeItemPointer(new CompletionItem(DeclarationPointer(d)));
    }
    return items;
}



}
