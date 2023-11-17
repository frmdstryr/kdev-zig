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

#define STATIC_INDEXED_STR(v) static IndexedString indexed_##v(#v)


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
    m_data = IndexedString(name);
}

BuiltinTypeData::BuiltinTypeData(const BuiltinTypeData& rhs)
    : AbstractTypeData(rhs)
    , m_data(rhs.m_data)
    , m_value(rhs.m_value)
{
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

NodeIndex BuiltinType::valueNode() const
{
    return d_func()->m_value;
}

void BuiltinType::setValueNode(NodeIndex node)
{
    d_func_dynamic()->m_value = node;
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
    const auto *rhs = static_cast<const BuiltinType*>(_rhs);

    if (d_func()->m_value != rhs->d_func()->m_value)
        return false;
    return d_func()->m_data == rhs->d_func()->m_data;
}

BuiltinType::BuiltinType(QString name)
    : AbstractType(createData<BuiltinType>())
{
    d_func_dynamic()->setTypeClassId<BuiltinType>();
    setDataType(name);
}

QString BuiltinType::toString() const
{
    return d_func()->m_data.str();
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
    return KDevHash(AbstractType::hash())
        << d_func()->m_data.hash() << d_func()->m_value;
}

bool BuiltinType::isUnsigned() const
{
    // TODO: Set flags instead of doing this ?
    STATIC_INDEXED_STR(u8);
    STATIC_INDEXED_STR(u16);
    STATIC_INDEXED_STR(u32);
    STATIC_INDEXED_STR(u64);
    STATIC_INDEXED_STR(u128);
    STATIC_INDEXED_STR(usize);
    STATIC_INDEXED_STR(c_char);
    STATIC_INDEXED_STR(c_uint);
    STATIC_INDEXED_STR(c_ulong);
    STATIC_INDEXED_STR(c_ulonglong);

    const IndexedString &d = d_func()->m_data;
    return (d == indexed_u8
         || d == indexed_u16
         || d == indexed_u32
         || d == indexed_u64
         || d == indexed_u128
         || d == indexed_usize
         || isComptimeInt()
         || d == indexed_c_char
         || d == indexed_c_uint
         || d == indexed_c_ulong
         || d == indexed_c_ulonglong
         || unsignedIntPattern.match(toString()).hasMatch()
    );
}

bool BuiltinType::isSigned() const
{
    STATIC_INDEXED_STR(i8);
    STATIC_INDEXED_STR(i16);
    STATIC_INDEXED_STR(i32);
    STATIC_INDEXED_STR(i64);
    STATIC_INDEXED_STR(i128);
    STATIC_INDEXED_STR(isize);
    STATIC_INDEXED_STR(c_int);
    STATIC_INDEXED_STR(c_short);
    STATIC_INDEXED_STR(c_long);
    STATIC_INDEXED_STR(c_longlong);

    // TODO: Set flags instead of doing this ?
    const IndexedString &d = d_func()->m_data;
    return (d == indexed_i8
         || d == indexed_i16
         || d == indexed_i32
         || d == indexed_i64
         || d == indexed_i128
         || isComptimeInt()
         || d == indexed_isize
         || d == indexed_c_int
         || d == indexed_c_short
         || d == indexed_c_long
         || d == indexed_c_longlong
         || signedIntPattern.match(toString()).hasMatch()
    );
}

bool BuiltinType::isFloat() const
{
    STATIC_INDEXED_STR(f16);
    STATIC_INDEXED_STR(f32);
    STATIC_INDEXED_STR(f64);
    STATIC_INDEXED_STR(f80);
    STATIC_INDEXED_STR(f128);
    STATIC_INDEXED_STR(c_longdouble);
    // TODO: Set flags instead of doing this ?
    const IndexedString &d = d_func()->m_data;
    return (d == indexed_f32
         || d == indexed_f64
         || isComptime() // Can be int or float
         || d == indexed_f16
         || d == indexed_f80
         || d == indexed_f128
         || d == indexed_c_longdouble
    );
}

bool BuiltinType::isComptimeInt() const
{
    STATIC_INDEXED_STR(comptime_int);
    return d_func()->m_data == indexed_comptime_int;
}

bool BuiltinType::isComptimeFloat() const
{
    STATIC_INDEXED_STR(comptime_float);
    return d_func()->m_data == indexed_comptime_float;
}

bool BuiltinType::isBool() const
{
    STATIC_INDEXED_STR(bool);
    return d_func()->m_data == indexed_bool;
}

bool BuiltinType::isNull() const
{
    STATIC_INDEXED_STR(null);
    return d_func()->m_data == indexed_null;
}

bool BuiltinType::isType() const
{
    STATIC_INDEXED_STR(type);
    return d_func()->m_data == indexed_type;
}

bool BuiltinType::isAnytype() const
{
    STATIC_INDEXED_STR(anytype);
    return d_func()->m_data == indexed_anytype;
}

bool BuiltinType::isUndefined() const
{
    STATIC_INDEXED_STR(undefined);
    return d_func()->m_data == indexed_undefined;
}

bool BuiltinType::isVoid() const
{
    STATIC_INDEXED_STR(void);
    return d_func()->m_data == indexed_void;
}


int BuiltinType::bitsize() const
{
    if (isComptime()) {
        return -1; // Should this return 0 ?
    }
    else if (isNumeric()) {
        QString v = toString().mid(1);
        bool ok;
        int i = v.toInt(&ok);
        if (ok) {
            return i;
        }
    }
    else if (isVoid()) {
        return 0;
    }
    return -1;
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

#undef STATIC_INDEXED_STR
} // end namespace
