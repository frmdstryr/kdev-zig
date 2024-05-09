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

#include <language/duchain/builders/abstractusebuilder.h>
#include <language/duchain/types/structuretype.h>
#include <serialization/indexedstring.h>

#include "contextbuilder.h"
#include "zignode.h"
#include "kdevzigduchain_export.h"

#include "types/slicetype.h"
#include "types/enumtype.h"

namespace Zig
{

using UseBuilderBase = KDevelop::AbstractUseBuilder<ZigNode, QString, ContextBuilder>;

struct AssignmentCheckResult
{
    bool ok;
    bool mismatch;
    AbstractType::Ptr value;
};

class KDEVZIGDUCHAIN_EXPORT UseBuilder : public UseBuilderBase
{
public:
    UseBuilder(const KDevelop::IndexedString &document);
    ~UseBuilder() override = default;

    virtual VisitResult visitNode(const ZigNode &node, const ZigNode &parent) override;

    VisitResult visitContainerField(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIf(const ZigNode &node, const ZigNode &parent);
    VisitResult visitBuiltinCall(const ZigNode &node, const ZigNode &parent);
    VisitResult visitStructInit(const ZigNode &node, const ZigNode &parent);
    VisitResult visitStructInitDot(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayInit(const ZigNode &node, const ZigNode &parent);
    VisitResult visitAssign(const ZigNode &node, const ZigNode &parent);
    VisitResult visitFieldAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitArrayAccess(const ZigNode &node, const ZigNode &parent);
    VisitResult visitDeref(const ZigNode &node, const ZigNode &parent);
    VisitResult visitUnwrapOptional(const ZigNode &node, const ZigNode &parent);
    VisitResult visitTry(const ZigNode &node, const ZigNode &parent);
    VisitResult visitCatch(const ZigNode &node, const ZigNode &parent);
    VisitResult visitIdent(const ZigNode &node, const ZigNode &parent);
    VisitResult visitEnumLiteral(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSwitch(const ZigNode &node, const ZigNode &parent);
    VisitResult visitSwitchCase(const ZigNode &node, const ZigNode &parent);


    bool checkAndAddFnArgUse(
        const KDevelop::AbstractType::Ptr &argType,
        const uint32_t argIndex,
        const ZigNode &argValueNode,
        const ZigNode &callNode,
        QMap<KDevelop::IndexedString, KDevelop::AbstractType::Ptr> &resolvedArgTypes);

    bool checkAndAddStructInitUse(
        const KDevelop::StructureType::Ptr &structType,
        const ZigNode &structInitNode,
        const KDevelop::RangeInRevision &useRange);

    bool checkAndAddStructFieldUse(
        const KDevelop::StructureType::Ptr &structType,
        const QString &fieldName,
        const ZigNode &valueNode,
        const ZigNode &structInitNode,
        const KDevelop::RangeInRevision &useRange);

    bool checkAndAddArrayInitUse(
        const SliceType::Ptr &sliceType,
        const ZigNode &arrayInitNode,
        const KDevelop::RangeInRevision &useRange);

    bool checkAndAddArrayItemUse(
        const KDevelop::AbstractType::Ptr &itemType,
        const uint32_t itemIndex,
        const ZigNode &valueNode,
        const ZigNode &arrayInitNode,
        const KDevelop::RangeInRevision &useRange);

    bool checkAndAddEnumUse(
        const KDevelop::AbstractType::Ptr &accessed,
        const QString &enumName,
        const KDevelop::RangeInRevision &useRange);

    AssignmentCheckResult checkGenericAssignment(
        const KDevelop::AbstractType::Ptr &lhs,
        const ZigNode &node,
        const ZigNode &parent);

    KDevelop::IndexedString document;
    KDevelop::QualifiedIdentifier fullPath;
    KDevelop::QualifiedIdentifier currentPath;
    KDevelop::Declaration* currentFieldDeclaration = nullptr;
};

} // end namespace zig
