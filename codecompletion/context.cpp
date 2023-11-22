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


#include "../duchain/zignode.h"
#include "../duchain/helpers.h"

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
    qCDebug(KDEV_ZIG) << "Invoke completion content: " << m_text << "following:" << m_followingText;
    ZigCompletion completion(complete_expr(m_text.toUtf8(), m_followingText.toUtf8()));

    auto top = m_duContext->topContext();
    auto localContext = top->findContextAt(m_position);
    if (localContext) {
        if (completion.data()->result_type == CompletionField) {
            QString name = completion.data()->name;
            Declaration* decl = Helper::declarationForName(
                name,
                CursorInRevision::invalid(),
                DUChainPointer<const DUContext>(localContext)
            );
            if (decl && decl->internalContext()) {
                for(const auto decl : localContext->localDeclarations())
                {
                    if(!decl
                        || decl->identifier() == globalImportIdentifier()
                        || decl->identifier() == globalAliasIdentifier()
                        || decl->identifier() == Identifier())
                        continue;
                    items << CompletionTreeItemPointer(new CompletionItem(DeclarationPointer(decl)));
                }
            }
        }
    } else {
        for(const auto &it : top->allDeclarations(CursorInRevision::invalid(), top))
        {
            const auto decl = it.first;
            if(!decl
                || decl->identifier() == globalImportIdentifier()
                || decl->identifier() == globalAliasIdentifier()
                || decl->identifier() == Identifier())
                continue;
            items << CompletionTreeItemPointer(new CompletionItem(DeclarationPointer(decl.first)));
        }
    }
    return items;
}

}
