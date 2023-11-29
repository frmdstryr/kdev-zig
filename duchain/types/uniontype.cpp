/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "uniontype.h"

#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/abstracttype.h"
#include "builtintype.h"
#include <helpers.h>
#include "enumtype.h"

namespace Zig {

using namespace KDevelop;

UnionTypeData::UnionTypeData(const UnionTypeData& rhs)
    : UnionTypeBase::Data(rhs)
    , m_baseType(rhs.m_baseType)
    , m_dataType(rhs.m_dataType)
{
}

REGISTER_TYPE(UnionType);

UnionType::UnionType(const UnionType& rhs) : UnionTypeBase(copyData<UnionType>(*rhs.d_func()))
{
}

UnionType::UnionType(UnionTypeData& data) : UnionTypeBase(data)
{
}

UnionType::UnionType()
    : UnionTypeBase(createData<UnionType>())
{
}

AbstractType* UnionType::clone() const
{
    return new UnionType(*this);
}

// bool UnionType::equals(const AbstractType* _rhs) const
// {
//     if (!equalsIgnoringValue(_rhs))
//         return false;
//     Q_ASSERT(dynamic_cast<const UnionType*>(_rhs));
//     const auto* rhs = static_cast<const UnionType*>(_rhs);
//     if (d_func()->m_dataType != rhs->d_func()->m_dataType)
//         return false;
//     return ComptimeType::equals(rhs);
// }

bool UnionType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!UnionTypeBase::equalsIgnoringValue(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const UnionType*>(_rhs));
    const auto* rhs = static_cast<const UnionType*>(_rhs);
    return (
        (d_func()->m_baseType == rhs->d_func()->m_baseType)
        && (d_func()->m_dataType == rhs->d_func()->m_dataType)
    );

    // if (d_func()->m_id == rhs->d_func()->m_id)
    //     return true; // Same decl
    //
    // // Two values of same base type
    // if (d_func()->m_baseType && rhs->d_func()->m_baseType)
    //     return d_func()->m_baseType == rhs->d_func()->m_baseType;
    // // value to base
    // if (d_func()->m_baseType)
    //     return d_func()->m_baseType.abstractType()->equals(rhs);
    // // base to value
    // if (rhs->d_func()->m_baseType)
    //     return rhs->d_func()->m_baseType.abstractType()->equals(this);
    //
    // // Don't check data type here as it depends on value
    // // eg with const Payload = union {Int: u32, Float: f32};
    // // we want Payload.Int and Payload.Float to equal ignoring the value
    // // for a union.
    // //return (d_func()->m_dataType == rhs->d_func()->m_dataType);
    // return false;
}

bool UnionType::canValueBeAssigned(const AbstractType::Ptr &rhs) const
{
    // This handles two values of same enum or two comptime known enums
    // with the same parent enum
    // eg @TypeOf(Status.Ok) == @TypeOf(Status.Error)
    if (equalsIgnoringValue(rhs.data()))
        return true;

    // Otherwise we may have a comptime known value checking against
    // the base eg Status == @TypeOf(Status.Error)
    if (const auto v = rhs.dynamicCast<UnionType>()) {
        if (auto otherBaseType = v->baseType()) {
            return equalsIgnoringValue(otherBaseType.data());
        }
    }
    return false;
}

void UnionType::accept0(TypeVisitor* v) const
{
    if (v->visit(static_cast<const AbstractType*>(this))) {
        acceptType(d_func()->m_baseType.abstractType(), v);
        acceptType(d_func()->m_dataType.abstractType(), v);
    }
    //v->endVisit(reinterpret_cast<const PointerType*>(this));
}

void UnionType::exchangeTypes(TypeExchanger* exchanger)
{
    if (d_func()->m_baseType.abstractType())
        d_func_dynamic()->m_baseType = IndexedType(exchanger->exchange(d_func()->m_baseType.abstractType()));
    if (d_func()->m_dataType.abstractType())
        d_func_dynamic()->m_dataType = IndexedType(exchanger->exchange(d_func()->m_dataType.abstractType()));
}

UnionType::~UnionType()
{
}

AbstractType::Ptr UnionType::baseType() const
{
    return d_func()->m_baseType.abstractType();
}

void UnionType::setBaseType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_baseType = IndexedType(type);
}

AbstractType::Ptr UnionType::dataType() const
{
    return d_func()->m_dataType.abstractType();
}

void UnionType::setDataType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_dataType = IndexedType(type);
}

AbstractType::Ptr UnionType::enumType() const
{
    if (auto unionType = baseType().dynamicCast<UnionType>())
        return unionType->enumType();
    if (auto base = baseType().dynamicCast<BuiltinType>()) {
        if (base->toString() == QLatin1String("enum")) {
            return AbstractType::Ptr(const_cast<UnionType*>(this));
        }
    }
    return baseType().dynamicCast<EnumType>();
}

QString UnionType::toString() const
{
    const auto &id = qualifiedIdentifier();
    if (const auto unionType = baseType().dynamicCast<UnionType>()) {
        QString r = QString("%1.%2: %3").arg(
                unionType->toString(),
                id.last().toString(),
                dataType() ? dataType()->toString() : QLatin1String("<notype>")
        );
        if (isComptimeKnown()) {
            return QString("%1 = %2").arg(r, comptimeKnownValue().str());
        }
        return r;
    }
    // } else if (const auto enumType = baseType()) {
    //     // union(enum)
    //     // Should this be included ?
    //     return QString("%1(%2)").arg(ident, enumType->toString());
    // }
    return id.toString();
}

AbstractType::WhichType UnionType::whichType() const
{
    return TypeStructure;
}

uint UnionType::hash() const
{
    return KDevHash(AbstractType::hash())
        << d_func()->m_baseType.hash()
        << d_func()->m_dataType.hash() << ComptimeType::hash();
}
}



