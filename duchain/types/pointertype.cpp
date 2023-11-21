/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "pointertype.h"


#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/abstracttype.h"
#include <helpers.h>

namespace Zig {

using namespace KDevelop;

PointerTypeData::PointerTypeData()
{
}

PointerTypeData::PointerTypeData(const PointerTypeData& rhs)
    : ComptimeTypeBase::Data(rhs)
    , m_baseType(rhs.m_baseType)
{
}

REGISTER_TYPE(PointerType);

PointerType::PointerType(const PointerType& rhs) : ComptimeTypeBase(copyData<PointerType>(*rhs.d_func()))
{
}

PointerType::PointerType(PointerTypeData& data) : ComptimeTypeBase(data)
{
}

PointerType::PointerType()
    : ComptimeTypeBase(createData<PointerType>())
{
}

AbstractType* PointerType::clone() const
{
    return new PointerType(*this);
}

bool PointerType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const PointerType*>(_rhs));
    const auto* rhs = static_cast<const PointerType*>(_rhs);

    return d_func()->m_baseType == rhs->d_func()->m_baseType;
}

bool PointerType::canValueBeAssigned(const AbstractType::Ptr &rhs) const
{
    if (equalsIgnoringValue(rhs.data()))
        return true;
    if (const auto v = rhs.dynamicCast<PointerType>()) {
        // Types not equal, try without modifiers
        if (modifiers() & (AbstractType::ConstModifier | AbstractType::VolatileModifier)) {
            return Helper::typesEqualIgnoringModifiers(baseType(), v->baseType());
        }
        // Else they should be equal which was already checked
    }
    return false;
}

void PointerType::accept0(TypeVisitor* v) const
{
    if (v->visit(this))
        acceptType(d_func()->m_baseType.abstractType(), v);

    //v->endVisit(reinterpret_cast<const KDevelop::PointerType*>(this));
}

void PointerType::exchangeTypes(TypeExchanger* exchanger)
{
    d_func_dynamic()->m_baseType = IndexedType(exchanger->exchange(d_func()->m_baseType.abstractType()));
}

PointerType::~PointerType()
{
}

AbstractType::Ptr PointerType::baseType() const
{
    return d_func()->m_baseType.abstractType();
}

void PointerType::setBaseType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_baseType = IndexedType(type);
}

QString PointerType::toString() const
{
    auto T = baseType();
    const auto isArray = modifiers() & ArrayModifier;
    QString baseString = T ? T->toString() : QStringLiteral("<notype>");
    QString prefix = isArray ? QStringLiteral("[*]") : QStringLiteral("*");
    return prefix + AbstractType::toString() + baseString;
}

AbstractType::WhichType PointerType::whichType() const
{
    return TypePointer;
}

uint PointerType::hash() const
{
    return KDevHash(AbstractType::hash())
        << d_func()->m_baseType.hash() << ComptimeType::hash();
}
}

