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
#include <QString>
#include "contextbuilder.h"
#include "zignode.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

using UseBuilderBase = KDevelop::AbstractUseBuilder<ZigNode, QString, ContextBuilder>;

class KDEVZIGDUCHAIN_EXPORT UseBuilder : public UseBuilderBase
{
public:
    UseBuilder(const KDevelop::IndexedString &document);
    ~UseBuilder() override = default;

    VisitResult visitNode(ZigNode &node, ZigNode &parent) override;

private:
    void visitCall(ZigNode &node, ZigNode &parent);
    void visitContainerInit(ZigNode &node, ZigNode &parent);
    void visitVarAccess(ZigNode &node, ZigNode &parent);
    void visitFieldAccess(ZigNode &node, ZigNode &parent);
    void visitArrayAccess(ZigNode &node, ZigNode &parent);
    void visitPtrAccess(ZigNode &node, ZigNode &parent);
    void visitLiteral(ZigNode &node, ZigNode &parent);
    void visitIdent(ZigNode &node, ZigNode &parent);

    KDevelop::IndexedString document;
    KDevelop::QualifiedIdentifier fullPath;
    KDevelop::QualifiedIdentifier currentPath;
};

}

#endif // USEBUILDER_H
