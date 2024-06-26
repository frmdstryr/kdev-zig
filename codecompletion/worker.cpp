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

#include "worker.h"
#include "model.h"
#include "context.h"

namespace Zig
{

CompletionWorker::CompletionWorker(CompletionModel *parent)
    : KDevelop::CodeCompletionWorker(parent)
{
}

KDevelop::CodeCompletionContext *CompletionWorker::createCompletionContext(const KDevelop::DUContextPointer &context,
                                                                           const QString &contextText,
                                                                           const QString &followingText,
                                                                           const KDevelop::CursorInRevision &position) const
{
    if (!context) {
        return nullptr;
    }

    return new CompletionContext(context, contextText, followingText, position, 0);
}

}
