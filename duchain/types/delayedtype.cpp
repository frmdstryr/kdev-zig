/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <language/duchain/types/typeregister.h>
#include <language/duchain/types/typesystem.h>
#include "delayedtype.h"
#include <QHash>

namespace Zig {

DelayedTypeData::DelayedTypeData(const DelayedTypeData& rhs)
    : ComptimeTypeBase::Data(rhs)
    , m_identifier(rhs.m_identifier)
{
}


REGISTER_TYPE(DelayedType);

DelayedType::DelayedType(DelayedTypeData& data) : ComptimeTypeBase(data)
{
}

DelayedType::DelayedType() : ComptimeTypeBase(createData<DelayedType>())
{
}

DelayedType::DelayedType(const DelayedType& rhs)
    : ComptimeTypeBase(copyData<DelayedType>(*rhs.d_func()))
{
}

AbstractType* DelayedType::clone() const
{
    return new DelayedType(*this);
}

bool DelayedType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const DelayedType*>(_rhs));
    const auto* rhs = static_cast<const DelayedType*>(_rhs);

    return d_func()->m_identifier == rhs->d_func()->m_identifier;
}

AbstractType::WhichType DelayedType::whichType() const
{
    return AbstractType::TypeDelayed;
}

QString DelayedType::toString() const
{
    return AbstractType::toString(false) + identifier().str();
}


void DelayedType::setIdentifier(const IndexedString& identifier)
{
    d_func_dynamic()->m_identifier = identifier;
}

IndexedString DelayedType::identifier() const
{
    return d_func()->m_identifier;
}

void DelayedType::accept0(KDevelop::TypeVisitor* v) const
{
    v->visit(this);
/*    v->endVisit(this);*/
}

uint DelayedType::hash() const
{
    return KDevHash(AbstractType::hash())
        << d_func()->m_identifier.hash() << ComptimeType::hash();
}
}

