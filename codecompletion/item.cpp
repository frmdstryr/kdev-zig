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
#include <language/codecompletion/codecompletionmodel.h>
#include <language/duchain/declaration.h>
#include <language/duchain/functiondeclaration.h>
#include <language/duchain/classdeclaration.h>
#include <language/duchain/classfunctiondeclaration.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/types/abstracttype.h>
#include <language/duchain/types/structuretype.h>
#include <QLabel>

#include "item.h"

namespace Zig
{

CompletionItem::CompletionItem(const KDevelop::DeclarationPointer& decl, int inheritanceDepth)
    : NormalDeclarationCompletionItem(decl, QExplicitlySharedDataPointer<KDevelop::CodeCompletionContext>(), inheritanceDepth)
{

}

QWidget* CompletionItem::createExpandingWidget(const KDevelop::CodeCompletionModel* model) const
{
    Q_UNUSED(model);
    // TODO: No idea why this is being called in the NormalDeclarationCompletionItem
    // this is a workaround
    return new QLabel();
}

bool CompletionItem::createsExpandingWidget() const
{
    return false;
}

}
