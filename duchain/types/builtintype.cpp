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
#include "../kdevzigastparser.h"

namespace Zig {


static QRegularExpression signedIntPattern("i\\d+");
static QRegularExpression unsignedIntPattern("u\\d+");
// static QRegularExpression floatPattern("f(16|32|64|80|128)");

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
        m_dataTypeLen = len;
        memcpy(m_dataType, data, m_dataTypeLen);
    }
}

BuiltinTypeData::BuiltinTypeData(const BuiltinTypeData& rhs)
    :AbstractTypeData(rhs)
{
    m_dataTypeLen = std::min(static_cast<size_t>(rhs.m_dataTypeLen), sizeof(m_dataType));
    memcpy(m_dataType, rhs.m_dataType, m_dataTypeLen);
}

REGISTER_TYPE(BuiltinType);

BuiltinType::BuiltinType(const BuiltinType& rhs)
    : AbstractType(copyData<BuiltinType>(*rhs.d_func()))
{
}

BuiltinType::BuiltinType(BuiltinTypeData& data)
    : AbstractType(data)
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

    Q_ASSERT(dynamic_cast<const BuiltinType*>(_rhs));
    const auto *rhs = static_cast<const BuiltinType*>(_rhs);

    if (d_func()->m_dataTypeLen != rhs->d_func()->m_dataTypeLen)
        return false;
    const auto n = std::min(
        static_cast<size_t>(d_func()->m_dataTypeLen),
        sizeof(d_func()->m_dataType)
    );
    return memcmp(d_func()->m_dataType, rhs->d_func()->m_dataType, n) == 0;
}

BuiltinType::BuiltinType(QString name)
    : AbstractType(createData<BuiltinType>())
{
    d_func_dynamic()->setTypeClassId<BuiltinType>();
    setDataType(name);
}

QString BuiltinType::toString() const
{
    const auto n = std::min(
        static_cast<size_t>(d_func()->m_dataTypeLen),
        sizeof(d_func()->m_dataType)
    );
    // No builtins are utf-8
    return QString::fromLatin1(d_func()->m_dataType, n);
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

bool BuiltinType::isUnsigned() const
{
    // TODO: Set flags instead of doing this ?
    QString n = toString();
    return (
        unsignedIntPattern.match(n).hasMatch()
        || n == QLatin1String("usize")
        || n == QLatin1String("c_char")
        || n == QLatin1String("c_uint")
        || n == QLatin1String("c_ulong")
        || n == QLatin1String("c_ulonglong")
        || n == QLatin1String("comptime_int")
    );
}

bool BuiltinType::isSigned() const
{
    // TODO: Set flags instead of doing this ?
    QString n = toString();
    return (
        signedIntPattern.match(n).hasMatch()
        || n == QLatin1String("isize")
        || n == QLatin1String("c_int")
        || n == QLatin1String("c_short")
        || n == QLatin1String("c_long")
        || n == QLatin1String("c_longlong")
        || n == QLatin1String("comptime_int")
    );
}

bool BuiltinType::isFloat() const
{
    // TODO: Set flags instead of doing this ?
    QString n = toString();
    return (n == QLatin1String("f32")
        || n == QLatin1String("f64")
        || n == QLatin1String("f128")
        || n == QLatin1String("f16")
        || n == QLatin1String("f80")
        || n == QLatin1String("comptime_float")
        || n == QLatin1String("comptime_int") // automatically casted
        || n == QLatin1String("c_longdouble")
    );
}


bool BuiltinType::isBuiltinFunc(const QString& name)
{
   return is_zig_builtin_fn_name(name.toUtf8());
}

bool BuiltinType::isBuiltinType(const QString& name)
{
    // TODO: Pull these from zig directly somehow
    return (
        name == QLatin1String("u8") // Very common
        || name == QLatin1String("void")
        || name == QLatin1String("type")
        || name == QLatin1String("bool")
        || name == QLatin1String("isize")
        || name == QLatin1String("usize")
        || name == QLatin1String("comptime_int")
        || name == QLatin1String("comptime_float")
        || unsignedIntPattern.match(name).hasMatch()
        || signedIntPattern.match(name).hasMatch()
        || name == QLatin1String("f32")
        || name == QLatin1String("f64")
        || name == QLatin1String("f128")
        || name == QLatin1String("f16")
        || name == QLatin1String("f80")
        || name == QLatin1String("anyerror")
        || name == QLatin1String("anyframe")
        || name == QLatin1String("anytype")
        || name == QLatin1String("noreturn")
        || name == QLatin1String("anyopaque")
        || name == QLatin1String("null")
        || name == QLatin1String("undefined")
        || name == QLatin1String("c_char")
        || name == QLatin1String("c_short")
        || name == QLatin1String("c_ushort")
        || name == QLatin1String("c_int")
        || name == QLatin1String("c_uint")
        || name == QLatin1String("c_long")
        || name == QLatin1String("c_ulong")
        || name == QLatin1String("c_longlong")
        || name == QLatin1String("c_ulonglong")
        || name == QLatin1String("c_longdouble")
    );
}

bool BuiltinType::isBuiltinVariable(const QString& name)
{
    // TODO: Pull these from zig directly somehow
    return (
        name == QLatin1String("null")
        || name == QLatin1String("undefined")
        || name == QLatin1String("true")
        || name == QLatin1String("false")
    );
}

AbstractType::Ptr BuiltinType::newFromName(const QString& name)
{
    static QHash<QString, AbstractType::Ptr> builtinTypeCache;
    auto it = builtinTypeCache.constFind(name);
    if (it != builtinTypeCache.constEnd())
        return *it;
    if (BuiltinType::isBuiltinType(name)) {
        auto t = new BuiltinType(name);
        Q_ASSERT(t);
        auto r = AbstractType::Ptr(t);
        builtinTypeCache.insert(name, r);
        return r;
    }
    if (name == QLatin1String("true") || name == QLatin1String("false")) {
        return newFromName("bool");
    }
    return AbstractType::Ptr();
}


}
