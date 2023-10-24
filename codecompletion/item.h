/*
 * Copyright 2023  frmdstryr <frmdstryr@protonmail.com>
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

#include <language/codecompletion/normaldeclarationcompletionitem.h>
#include "kdevzigcompletion_export.h"

namespace Zig
{

class KDEVZIGCOMPLETION_EXPORT CompletionItem : public KDevelop::NormalDeclarationCompletionItem
{
public:
    CompletionItem(
        const KDevelop::DeclarationPointer& decl,
        int inheritanceDepth = 0);

protected:
    virtual QWidget* createExpandingWidget(const KDevelop::CodeCompletionModel* model) const override;
    virtual bool createsExpandingWidget() const override;
};
}
