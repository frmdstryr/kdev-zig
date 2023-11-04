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
static QRegularExpression floatPattern("f(16|32|64|80|128)");

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

QString BuiltinType::toString() const
{
    const auto n = std::min(
        static_cast<size_t>(d_func()->m_dataTypeLen),
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

bool BuiltinType::isUnsigned() const
{
    // TODO: Set flags instead of doing this ?
    QString n = toString();
    return (
        unsignedIntPattern.match(n).hasMatch()
        || n == "usize"
        || n == "c_char"
        || n == "c_uint"
        || n == "c_ulong"
        || n == "c_ulonglong"
        || n == "comptime_int"
    );
}

bool BuiltinType::isSigned() const
{
    // TODO: Set flags instead of doing this ?
    QString n = toString();
    return (
        signedIntPattern.match(n).hasMatch()
        || n == "isize"
        || n == "c_int"
        || n == "c_short"
        || n == "c_long"
        || n == "c_longlong"
        || n == "comptime_int"
    );
}

bool BuiltinType::isFloat() const
{
    // TODO: Set flags instead of doing this ?
    QString n = toString();
    return (
        floatPattern.match(n).hasMatch()
        || n == "comptime_float"
        || n == "comptime_int" // automatically casted
        || n == "c_longdouble"
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
        name == "void"
        || name == "type"
        || name == "bool"
        || name == "isize"
        || name == "usize"
        || name == "anyerror"
        || name == "anyframe"
        || name == "noreturn"
        || name == "anyopaque"
        || name == "comptime_int"
        || name == "comptime_float"
        || name == "c_char"
        || name == "c_short"
        || name == "c_ushort"
        || name == "c_int"
        || name == "c_uint"
        || name == "c_long"
        || name == "c_ulong"
        || name == "c_longlong"
        || name == "c_ulonglong"
        || name == "c_longdouble"
        || unsignedIntPattern.match(name).hasMatch()
        || signedIntPattern.match(name).hasMatch()
        || floatPattern.match(name).hasMatch()
    );
}

bool BuiltinType::isBuiltinVariable(const QString& name)
{
    // TODO: Pull these from zig directly somehow
    return (
        name == "null"
        || name == "undefined"
        || name == "true"
        || name == "false"
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
    if (name == "true" || name == "false") {
        return newFromName("bool");
    }
    return AbstractType::Ptr();
}


}
