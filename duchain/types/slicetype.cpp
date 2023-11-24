/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slicetype.h"


#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/integraltype.h"
#include "language/duchain/types/structuretype.h"

#include "language/duchain/types/arraytype.h"
#include <helpers.h>

namespace Zig {

using namespace KDevelop;

SliceTypeData::SliceTypeData(const SliceTypeData& rhs)
    : ComptimeTypeBase::Data(rhs)
    , m_dimension(rhs.m_dimension)
    , m_sentinel(rhs.m_sentinel)
    , m_elementType(rhs.m_elementType)
{
}

REGISTER_TYPE(SliceType);

SliceType::SliceType(const SliceType& rhs)
    : ComptimeTypeBase(copyData<SliceType>(*rhs.d_func()))
{
}

SliceType::SliceType(SliceTypeData& data)
    : ComptimeTypeBase(data)
{
}

SliceType::SliceType()
    : ComptimeTypeBase(createData<SliceType>())
{
}

AbstractType* SliceType::clone() const
{
    return new SliceType(*this);
}

bool SliceType::equalsIgnoringValueAndDimension(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;
    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;
    Q_ASSERT(dynamic_cast<const SliceType*>(_rhs));
    const auto* rhs = static_cast<const SliceType*>(_rhs);
    TYPE_D(SliceType);
    if (d->m_sentinel != rhs->d_func()->m_sentinel)
        return false;
    return d->m_elementType == rhs->d_func()->m_elementType;

}

bool SliceType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (!equalsIgnoringValueAndDimension(_rhs))
        return false;
    const auto* rhs = static_cast<const SliceType*>(_rhs);
    // Check dimension
    return d_func()->m_dimension == rhs->d_func()->m_dimension;
}


int SliceType::dimension() const
{
    return d_func()->m_dimension;
}

void SliceType::setDimension(int dimension)
{
    d_func_dynamic()->m_dimension = dimension;
}

AbstractType::Ptr SliceType::elementType() const
{
    return d_func()->m_elementType.abstractType();
}

void SliceType::setSentinel(int sentinel) {
    d_func_dynamic()->m_sentinel = sentinel;
}

int SliceType::sentinel() const {
    return d_func()->m_sentinel;
}

void SliceType::setElementType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_elementType = IndexedType(type);
}

QString SliceType::toString() const
{
    // TODO: Clean this up...
    const auto T = elementType();
    QString s = (sentinel() >= 0) ? QStringLiteral(":%1").arg(sentinel()) : "";
    QString type = T ? T->toString() : QStringLiteral("<notype>");
    QString v = isComptimeKnown() ? QString(" = \"%1\"").arg(comptimeKnownValue().str()) : "";
    if (d_func()->m_dimension == 0) {
        return AbstractType::toString() + QStringLiteral("[%1]%2%3").arg(s).arg(type).arg(v);
    }
    return AbstractType::toString() + QStringLiteral("[%1%2]%3%4").arg(d_func()->m_dimension).arg(s).arg(type).arg(v);
}

void SliceType::accept0(TypeVisitor* v) const
{
    if (v->visit(this)) {
        acceptType(d_func()->m_elementType.abstractType(), v);
    }

    // v->endVisit(reinterpret_cast<const ArrayType*>(this));
}

void SliceType::exchangeTypes(TypeExchanger* exchanger)
{
    d_func_dynamic()->m_elementType = IndexedType(exchanger->exchange(d_func()->m_elementType.abstractType()));
}

AbstractType::WhichType SliceType::whichType() const
{
    return TypeUnsure;
}

uint SliceType::hash() const
{
    return KDevHash(AbstractType::hash())
           << (elementType() ? elementType()->hash() : 0)
           << dimension() << sentinel() << ComptimeType::hash();
}

bool SliceType::canValueBeAssigned(const AbstractType::Ptr &rhs)  const
{
    if (equalsIgnoringValue(rhs.data()))
        return true;

    if (d_func()->m_dimension != 0)
        return false;
    // if target is const a non const can be used
    if (elementType()->modifiers() & AbstractType::ConstModifier) {
        if (const auto v = rhs.dynamicCast<SliceType>()) {
            return Helper::typesEqualIgnoringModifiers(elementType(), v->elementType());
        }

        if (const auto ptr = rhs.dynamicCast<PointerType>()) {
            if (const auto v = ptr->baseType().dynamicCast<SliceType>()) {
                // TODO: const and sentinel ?
                return Helper::typesEqualIgnoringModifiers(elementType(), v->elementType());
            }
        }
    }

    return false;

}

} // End namespace
