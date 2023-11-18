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

bool SliceType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;
    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;
    Q_ASSERT(dynamic_cast<const SliceType*>(_rhs));
    const auto* rhs = static_cast<const SliceType*>(_rhs);
    TYPE_D(SliceType);
    if (d->m_dimension != rhs->d_func()->m_dimension)
        return false;
    if (d->m_sentinel != rhs->d_func()->m_sentinel)
        return false;
    return d->m_elementType == rhs->d_func()->m_elementType;

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
    const auto isConst = T ? T->modifiers() & ConstModifier : false;
    QString c = isConst ? "const " : "";
    QString s = (sentinel() >= 0) ? QStringLiteral(":%1").arg(sentinel()) : "";
    QString type = T ? (c + T->toString()) : QStringLiteral("<notype>");
    QString v = isComptimeKnown() ? QString(" = \"%1\"").arg(comptimeKnownValue().str()) : "";
    if (d_func()->m_dimension == 0) {
        return QStringLiteral("[%1]%2%3").arg(s).arg(type).arg(v);
    }
    return QStringLiteral("[%1%2]%3%4").arg(d_func()->m_dimension).arg(s).arg(type).arg(v);
}

void SliceType::accept0(TypeVisitor* v) const
{
    if (v->visit(this)) {
        acceptType(d_func()->m_elementType.abstractType(), v);
    }

    // v->endVisit(reinterpret_cast<const ArrayType*>(this));
}

// void SliceType::exchangeTypes(TypeExchanger* exchanger)
// {
//     TYPE_D_DYNAMIC(SliceType);
//     d->m_elementType = IndexedType(exchanger->exchange(d->m_elementType.abstractType()));
// }

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
}
