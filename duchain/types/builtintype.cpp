/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include <QRegularExpression>

#include "builtintype.h"

namespace Zig {


using namespace KDevelop;

BuiltinTypeData::BuiltinTypeData()
{
}

BuiltinTypeData::BuiltinTypeData(const QString& name)
{
    setData(name);
}

void BuiltinTypeData::setData(const QString& name)
{
    QByteArray data = name.toUtf8();
    if (data.size() > 0) {
        size_t len = std::min(static_cast<size_t>(data.size()), sizeof(m_dataType));
        Q_ASSERT(len <= sizeof(m_dataType));
        memcpy(m_dataType, data, len);
    }
}

BuiltinTypeData::BuiltinTypeData(const BuiltinTypeData& rhs)
    :AbstractTypeData(rhs)
{
    memcpy(m_dataType, rhs.m_dataType, sizeof(m_dataType));
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
    return toString();
}

void BuiltinType::setDataType(QString &dataType)
{
    d_func_dynamic()->setData(dataType);
}

AbstractType* BuiltinType::clone() const
{
    return new BuiltinType(toString());
}

bool BuiltinType::equals(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!AbstractType::equals(_rhs))
        return false;

    if (const auto *rhs =dynamic_cast<const BuiltinType*>(_rhs))
        return toString() == rhs->toString();

    return false;
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
    const auto n = std::min(
        strlen(d_func()->m_dataType),
        sizeof(d_func()->m_dataType)
    );
    return QString::fromUtf8(d_func()->m_dataType, n);
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
    return KDevHash(AbstractType::hash()) << toString();
}

BuiltinType* BuiltinType::newFromName(const QString& name)
{
    static QRegularExpression unsignedIntPattern("u\\d+");
    static QRegularExpression signedIntPattern("i\\d+");
    static QRegularExpression floatPattern("f(16|32|64|80|128)");
    if (name == "void")
        return new BuiltinType("void");
    if (name == "type")
        return new BuiltinType("type");
    if (name == "bool")
        return new BuiltinType("bool");
    if (name == "isize")
        return new BuiltinType("isize");
    if (name == "usize")
        return new BuiltinType("usize");
    if (name == "type")
        return new BuiltinType("type");
    if (name == "anyerror")
        return new BuiltinType("anyerror");
    if (name == "noreturn")
        return new BuiltinType("noreturn");
    if (name == "anyopaque")
        return new BuiltinType("anyopaque");
    if (name == "comptime_int")
        return new BuiltinType("comptime_int");
    if (name == "comptime_float")
        return new BuiltinType("comptime_float");
    if (unsignedIntPattern.match(name).hasMatch())
        return new BuiltinType(name);
    if (signedIntPattern.match(name).hasMatch())
        return new BuiltinType(name);
    if (floatPattern.match(name).hasMatch())
        return new BuiltinType(name);
    // TODO: c_ types
    return nullptr;
}



}
