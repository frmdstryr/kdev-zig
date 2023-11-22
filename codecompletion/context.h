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
#pragma once

#include <QString>

#include <language/codecompletion/codecompletioncontext.h>

#include <kdevzigcompletion_export.h>

#include "types/comptimetype.h"
#include "types/pointertype.h"
#include "types/builtintype.h"
#include "types/optionaltype.h"
#include "types/slicetype.h"
#include "types/enumtype.h"
#include "types/uniontype.h"

namespace Zig
{

class KDEVZIGCOMPLETION_EXPORT CompletionContext : public KDevelop::CodeCompletionContext
{
public:
    CompletionContext(KDevelop::DUContextPointer context,
                      const QString &contextText,
                      const QString &followingText,
                      const KDevelop::CursorInRevision &position,
                      int depth = 0);

    QList<KDevelop::CompletionTreeItemPointer> completionItems(bool &abort, bool fullCompletion) override;

    QList<KDevelop::CompletionTreeItemPointer> completionsForStruct(const StructureType::Ptr &t) const;
    QList<KDevelop::CompletionTreeItemPointer> completionsForEnum(const EnumType::Ptr &t) const;
    QList<KDevelop::CompletionTreeItemPointer> completionsForSlice(const SliceType::Ptr &t) const;
    QList<KDevelop::CompletionTreeItemPointer> completionsForUnion(const UnionType::Ptr &t) const;

    QList<KDevelop::CompletionTreeItemPointer> completionsFromLocalDecls(const DUContextPointer &ctx) const;

private:
    QString m_followingText;
};

}
