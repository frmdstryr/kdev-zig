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

#include "zigdebug.h"
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

BuiltinTypeData::BuiltinTypeData(const BuiltinTypeData& rhs)
    : ComptimeTypeBase::Data(rhs)
    , m_data(rhs.m_data)
{
}

REGISTER_TYPE(BuiltinType);

BuiltinType::BuiltinType(const BuiltinType& rhs)
    : ComptimeTypeBase(copyData<BuiltinType>(*rhs.d_func()))
{
}

BuiltinType::BuiltinType(BuiltinTypeData& data)
    : ComptimeTypeBase(data)
{
}

BuiltinType::BuiltinType(const IndexedString &name)
    : ComptimeTypeBase(createData<BuiltinType>())
{
    // d_func_dynamic()->setTypeClassId<BuiltinType>();
    setDataType(name);
}

BuiltinType::BuiltinType(const QString &name)
    : BuiltinType(IndexedString(name))
{
}

const IndexedString& BuiltinType::dataType() const
{
    return d_func()->m_data;
}

void BuiltinType::setDataType(const IndexedString &dataType)
{
    STATIC_INDEXED_STR(null);
    STATIC_INDEXED_STR(void);
    STATIC_INDEXED_STR(true);
    STATIC_INDEXED_STR(false);
    STATIC_INDEXED_STR(bool);
    if (dataType == indexed_true || dataType == indexed_false)
        d_func_dynamic()->m_data = indexed_bool;
    else
        d_func_dynamic()->m_data = dataType;

    // These are always comptime known
    if (dataType == indexed_null
        || dataType == indexed_void
        || dataType == indexed_true
        || dataType == indexed_false
    ) {
        setComptimeKnownValue(dataType);
    }
}


AbstractType* BuiltinType::clone() const
{
    return new BuiltinType(*this);
}

bool BuiltinType::equalsIgnoringValue(const KDevelop::AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;
    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;
    const auto *rhs = static_cast<const BuiltinType*>(_rhs);
    return d_func()->m_data == rhs->d_func()->m_data;
}

bool BuiltinType::canValueBeAssigned(const AbstractType::Ptr &rhs) const
{
    if (equalsIgnoringValue(rhs.data()))
        return true;
    if (const auto v = rhs.dynamicCast<BuiltinType>()) {
        // Can assign non-const to const but not the other way
        if (dataType() == v->dataType())
            return true;
        if (isInteger() && v->isComptimeInt())
            return true;
        if (isFloat() && (v->isComptimeInt() || v->isComptimeFloat()))
            return true; // Auto casts
        if (
            (isSigned() && v->isSigned())
            || (isUnsigned() && v->isUnsigned())
        ) {
            const auto s1 = bitsize();
            const auto s2 = v->bitsize();
            return (s1 > 0 && s2 > 0 && s2 <= s1);
        }
        // Else do other cases need to cast?
        if (v->isUndefined() && !(isType() || isAnytype()))
            return true;
    }
    if (isType() || isAnytype())
        return true;

    return false;
}

QString BuiltinType::toString() const
{
    // AbstractType::toString() prints modifiers (eg const/volatile)
    if (!isComptimeKnown() || isVoid() || isNull()) {
        return AbstractType::toString() + d_func()->m_data.str();
    }
    return AbstractType::toString() + QString("%1 = %2").arg(
        d_func()->m_data.str(),
        comptimeKnownValue().str()
    );
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
        << d_func()->m_data.hash() << ComptimeType::hash();
}

bool BuiltinType::isChar() const
{
    STATIC_INDEXED_STR(u8);
    STATIC_INDEXED_STR(c_char);
    return (d_func()->m_data == indexed_u8
        || d_func()->m_data == indexed_c_char
    );
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
         || unsignedIntPattern.match(d.str()).hasMatch()
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
         || signedIntPattern.match(d.str()).hasMatch()
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
         || isComptimeInt()
         || isComptimeFloat()
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

bool BuiltinType::isTrue() const
{
    STATIC_INDEXED_STR(true);
    return d_func()->m_comptimeValue == indexed_true;
}

bool BuiltinType::isFalse() const
{
    STATIC_INDEXED_STR(false);
    return d_func()->m_comptimeValue == indexed_false;
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

bool BuiltinType::isAnyframe() const
{
    STATIC_INDEXED_STR(anyframe);
    return d_func()->m_data == indexed_anyframe;
}

bool BuiltinType::isFrame() const
{
    STATIC_INDEXED_STR(frame);
    return d_func()->m_data == indexed_frame;
}

bool BuiltinType::isOpaque() const
{
    STATIC_INDEXED_STR(opaque);
    return d_func()->m_data == indexed_opaque;
}

bool BuiltinType::isNoreturn() const
{
    STATIC_INDEXED_STR(noreturn);
    return d_func()->m_data == indexed_noreturn;
}

bool BuiltinType::isTrap() const
{
    STATIC_INDEXED_STR(trap);
    return d_func()->m_data == indexed_trap;
}

bool BuiltinType::isUnreachable() const
{
    STATIC_INDEXED_STR(unreachable);
    return d_func()->m_data == indexed_unreachable;
}

int BuiltinType::bitsize() const
{
    if (isVoid())
        return 0;
    if (isNumeric() && !(isComptimeInt() || isComptimeFloat())) {
        QString v = d_func()->m_data.str().mid(1);
        bool ok;
        int i = v.toInt(&ok);
        if (ok) {
            return i;
        }
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
        || name == QLatin1String("true")
        || name == QLatin1String("false")
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
        || name == QLatin1String("trap") // @panic / @compileError
        || name == QLatin1String("unreachable")
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
        auto t = AbstractType::Ptr(new BuiltinType(name));
        builtinTypeCache.insert(name, t);
        return t;
    }
    return AbstractType::Ptr();
}

#undef STATIC_INDEXED_STR
} // end namespace
