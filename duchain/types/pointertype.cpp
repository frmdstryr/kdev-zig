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

namespace Zig {

using namespace KDevelop;

PointerTypeData::PointerTypeData()
{
}

PointerTypeData::PointerTypeData(const PointerTypeData& rhs)
    : AbstractTypeData(rhs)
    , m_baseType(rhs.m_baseType)
{
}

REGISTER_TYPE(PointerType);

PointerType::PointerType(const PointerType& rhs) : AbstractType(copyData<PointerType>(*rhs.d_func()))
{
}

PointerType::PointerType(PointerTypeData& data) : AbstractType(data)
{
}

AbstractType* PointerType::clone() const
{
    return new PointerType(*this);
}

bool PointerType::equals(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!AbstractType::equals(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const PointerType*>(_rhs));
    const auto* rhs = static_cast<const PointerType*>(_rhs);

    return d_func()->m_baseType == rhs->d_func()->m_baseType;
}

PointerType::PointerType()
    : AbstractType(createData<PointerType>())
{
}

void PointerType::accept0(TypeVisitor* v) const
{
    if (v->visit(this))
        acceptType(d_func()->m_baseType.abstractType(), v);

    v->endVisit(reinterpret_cast<const KDevelop::PointerType*>(this));
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
    const auto isConst = T ? T->modifiers() & ConstModifier : false;
    QString c = isConst ? "const " : "";
    QString baseString = T ? (c + T->toString()) : QStringLiteral("<notype>");
    return QLatin1Char('*') + baseString + AbstractType::toString(true);
}

AbstractType::WhichType PointerType::whichType() const
{
    return TypePointer;
}

uint PointerType::hash() const
{
    return KDevHash(AbstractType::hash()) << d_func()->m_baseType.hash();
}
}
