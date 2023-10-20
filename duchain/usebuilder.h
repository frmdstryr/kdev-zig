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

#ifndef USEBUILDER_H
#define USEBUILDER_H

#include <language/duchain/builders/abstractusebuilder.h>
#include <serialization/indexedstring.h>

#include "contextbuilder.h"
#include "zignode.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

using UseBuilderBase = KDevelop::AbstractUseBuilder<ZigNode, ZigPath, ContextBuilder>;

class KDEVZIGDUCHAIN_EXPORT UseBuilder : public UseBuilderBase
{
public:
    UseBuilder(const KDevelop::IndexedString &document);
    ~UseBuilder() override = default;

    ZVisitResult visitNode(ZigNode *node, ZigNode *parent) override;

private:
    void visitPath(ZigNode *node, ZigNode *parent);
    void visitPathSegment(ZigNode *node, ZigNode *parent);

    KDevelop::QualifiedIdentifier fullPath;
    KDevelop::QualifiedIdentifier currentPath;

    KDevelop::IndexedString document;
};

}

#endif // USEBUILDER_H
