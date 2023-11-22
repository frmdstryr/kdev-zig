/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "enumtype.h"

#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/abstracttype.h"
#include "builtintype.h"
#include <helpers.h>

namespace Zig {

using namespace KDevelop;

EnumTypeData::EnumTypeData(const EnumTypeData& rhs)
    : EnumTypeBase::Data(rhs)
    , m_enumType(rhs.m_enumType)
{
}

REGISTER_TYPE(EnumType);

EnumType::EnumType(const EnumType& rhs) : EnumTypeBase(copyData<EnumType>(*rhs.d_func()))
{
}

EnumType::EnumType(EnumTypeData& data) : EnumTypeBase(data)
{
}

EnumType::EnumType()
    : EnumTypeBase(createData<EnumType>())
{
}

AbstractType* EnumType::clone() const
{
    return new EnumType(*this);
}

bool EnumType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!EnumTypeBase::equalsIgnoringValue(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const EnumType*>(_rhs));
    const auto* rhs = static_cast<const EnumType*>(_rhs);

    return d_func()->m_enumType == rhs->d_func()->m_enumType;
}

bool EnumType::canValueBeAssigned(const AbstractType::Ptr &rhs) const
{
    // This handles two values of same enum or two comptime known enums
    // with the same parent enum
    // eg @TypeOf(Status.Ok) == @TypeOf(Status.Error)
    if (equalsIgnoringValue(rhs.data()))
        return true;

    // Otherwise we may have a comptime known value checking against
    // the base eg Status == @TypeOf(Status.Error)
    if (const auto v = rhs.dynamicCast<EnumType>()) {
        if (auto otherBaseType = v->enumType()) {
            return equalsIgnoringValue(otherBaseType.data());
        }
    }
    return false;
}

void EnumType::accept0(TypeVisitor* v) const
{
    if (v->visit(static_cast<const AbstractType*>(this)))
        acceptType(d_func()->m_enumType.abstractType(), v);

    //v->endVisit(reinterpret_cast<const PointerType*>(this));
}

void EnumType::exchangeTypes(TypeExchanger* exchanger)
{
    if (d_func()->m_enumType.abstractType())
        d_func_dynamic()->m_enumType = IndexedType(exchanger->exchange(d_func()->m_enumType.abstractType()));
}

EnumType::~EnumType()
{
}

AbstractType::Ptr EnumType::enumType() const
{
    return d_func()->m_enumType.abstractType();
}

void EnumType::setEnumType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_enumType = IndexedType(type);
}

QString EnumType::toString() const
{
    const auto &id = qualifiedIdentifier();
    if (auto t = enumType().dynamicCast<EnumType>()) {
        QString name = id.last().toString();
        QString value = comptimeKnownValue().str();
        if (name != value) {
            return QString("%1.%2 = %3").arg(t->toString(), name, value);
        }
        return QString("%1.%2").arg(t->toString(), name);
    }
    return id.toString();
}

AbstractType::WhichType EnumType::whichType() const
{
    if (enumType()) {
        return TypeEnumerator;
    }
    return TypeEnumeration;
}

uint EnumType::hash() const
{
    return KDevHash(AbstractType::hash())
        << d_func()->m_enumType.hash() << ComptimeType::hash();
}
}


