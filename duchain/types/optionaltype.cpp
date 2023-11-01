/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "optionaltype.h"


#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/abstracttype.h"

namespace Zig {

using namespace KDevelop;

OptionalTypeData::OptionalTypeData()
{
}

OptionalTypeData::OptionalTypeData(const OptionalTypeData& rhs)
    : AbstractTypeData(rhs)
    , m_baseType(rhs.m_baseType)
{
}

REGISTER_TYPE(OptionalType);

OptionalType::OptionalType(const OptionalType& rhs) : AbstractType(copyData<OptionalType>(*rhs.d_func()))
{
}

OptionalType::OptionalType(OptionalTypeData& data) : AbstractType(data)
{
}

AbstractType* OptionalType::clone() const
{
    return new OptionalType(*this);
}

bool OptionalType::equals(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!AbstractType::equals(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const OptionalType*>(_rhs));
    const auto* rhs = static_cast<const OptionalType*>(_rhs);

    return d_func()->m_baseType == rhs->d_func()->m_baseType;
}

OptionalType::OptionalType()
    : AbstractType(createData<OptionalType>())
{
}

void OptionalType::accept0(TypeVisitor* v) const
{
    if (v->visit(this))
        acceptType(d_func()->m_baseType.abstractType(), v);

    v->endVisit(reinterpret_cast<const PointerType*>(this));
}

void OptionalType::exchangeTypes(TypeExchanger* exchanger)
{
    d_func_dynamic()->m_baseType = IndexedType(exchanger->exchange(d_func()->m_baseType.abstractType()));
}

OptionalType::~OptionalType()
{
}

AbstractType::Ptr OptionalType::baseType() const
{
    return d_func()->m_baseType.abstractType();
}

void OptionalType::setBaseType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_baseType = IndexedType(type);
}

QString OptionalType::toString() const
{
    QString baseString = (baseType() ? baseType()->toString() : QStringLiteral("<notype>"));
    return QLatin1Char('?') + baseString + AbstractType::toString(true);
}

AbstractType::WhichType OptionalType::whichType() const
{
    return TypeUnsure;
}

uint OptionalType::hash() const
{
    return KDevHash(AbstractType::hash()) << d_func()->m_baseType.hash();
}
}
