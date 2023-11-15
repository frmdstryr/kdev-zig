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

    virtual VisitResult visitNode(const ZigNode &node, const ZigNode &parent) override;

private:
    VisitResult visitCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIf(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBuiltinCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitStructInit(const ZigNode &node, const ZigNode &parent);
    VisitResult visitAssign(const ZigNode &node, const ZigNode &parent);
    VisitResult visitVarAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitFieldAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitDeref(const ZigNode &node, const ZigNode &parent);
    VisitResult visitUnwrapOptional(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIdent(const ZigNode &node, const ZigNode &parent);
    VisitResult visitEnumLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSwitch(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSwitchCase(const ZigNode &node, const ZigNode &parent);

    bool checkAndAddEnumUse(
        const KDevelop::AbstractType::Ptr &accessed,
        const QString &enumName,
        const KDevelop::RangeInRevision &useRange);

    KDevelop::IndexedString document;
    KDevelop::QualifiedIdentifier fullPath;
    KDevelop::QualifiedIdentifier currentPath;
};

}

#endif // USEBUILDER_H
