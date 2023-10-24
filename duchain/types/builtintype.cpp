/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"

#include "builtintype.h"

namespace Zig {


using namespace KDevelop;

BuiltinTypeData::BuiltinTypeData()
{
}

BuiltinTypeData::BuiltinTypeData(const QString& name)
{
    m_dataType = name;
}

BuiltinTypeData::BuiltinTypeData(const BuiltinTypeData& rhs)
    :AbstractTypeData(rhs), m_dataType(rhs.m_dataType)
{

}

REGISTER_TYPE(BuiltinType);

BuiltinType::BuiltinType(const BuiltinType& rhs) : AbstractType(copyData<BuiltinType>(*rhs.d_func()))
{
}

BuiltinType::BuiltinType(BuiltinTypeData& data) : AbstractType(data)
{
}

QString BuiltinType::dataType() const
{
    return d_func()->m_dataType;
}

void BuiltinType::setDataType(QString &dataType)
{
    d_func_dynamic()->m_dataType = dataType;
}

AbstractType* BuiltinType::clone() const
{
    return new BuiltinType(*this);
}

bool BuiltinType::equals(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!AbstractType::equals(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const BuiltinType*>(_rhs));
    const auto* rhs = static_cast<const BuiltinType*>(_rhs);
    if (!d_func() || !rhs->d_func())
        return false;
    return d_func()->m_dataType == rhs->d_func()->m_dataType;
}

BuiltinType::BuiltinType(QString name)
    : AbstractType(createData<BuiltinType>())
{
    d_func_dynamic()->setTypeClassId<BuiltinType>();
    setDataType(name);
}

BuiltinType::~BuiltinType()
{
}

QString BuiltinType::toString() const
{
    return d_func()->m_dataType;
}

void BuiltinType::accept0(TypeVisitor* v) const
{
    v->visit(this);
}

AbstractType::WhichType BuiltinType::whichType() const
{
    return TypeIntegral;
}

uint BuiltinType::hash() const
{
    return KDevHash(AbstractType::hash()) << d_func()->m_dataType;
}


}
