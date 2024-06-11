/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "errortype.h"


#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/abstracttype.h"
#include "builtintype.h"
#include "helpers.h"

namespace Zig {

using namespace KDevelop;

ErrorTypeData::ErrorTypeData()
{
}

ErrorTypeData::ErrorTypeData(const ErrorTypeData& rhs)
    : ComptimeTypeBase::Data(rhs)
    , m_baseType(rhs.m_baseType)
    , m_errorType(rhs.m_errorType)
{
}

REGISTER_TYPE(ErrorType);

ErrorType::ErrorType(const ErrorType& rhs)
    : ComptimeTypeBase(copyData<ErrorType>(*rhs.d_func()))
{
}

ErrorType::ErrorType(ErrorTypeData& data)
    : ComptimeTypeBase(data)
{
}

ErrorType::ErrorType()
    : ComptimeTypeBase(createData<ErrorType>())
{
}

AbstractType* ErrorType::clone() const
{
    return new ErrorType(*this);
}

bool ErrorType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const ErrorType*>(_rhs));
    const auto* rhs = static_cast<const ErrorType*>(_rhs);

    return (
        (d_func()->m_baseType == rhs->d_func()->m_baseType)
        && (d_func()->m_errorType == rhs->d_func()->m_errorType)
    );
}

bool ErrorType::canValueBeAssigned(const AbstractType::Ptr &rhs, const KDevelop::IProject* project) const
{
    if (equalsIgnoringValue(rhs.data()))
        return true;
    if (const auto v = rhs.dynamicCast<BuiltinType>()) {
        if (v->isAnyerror())
            return true;
    }
    if (const auto v = rhs.dynamicCast<ErrorType>())
        return Helper::canTypeBeAssigned(baseType(), v->baseType(), project);
    return Helper::canTypeBeAssigned(baseType(), rhs, project);
}

void ErrorType::accept0(TypeVisitor* v) const
{
    if (v->visit(this)) {
        acceptType(d_func()->m_baseType.abstractType(), v);
        acceptType(d_func()->m_errorType.abstractType(), v);
    }
    // v->endVisit(this);
}

void ErrorType::exchangeTypes(TypeExchanger* exchanger)
{
    d_func_dynamic()->m_baseType = IndexedType(exchanger->exchange(d_func()->m_baseType.abstractType()));
    if (errorType().data()) {
        d_func_dynamic()->m_errorType = IndexedType(exchanger->exchange(d_func()->m_errorType.abstractType()));
    }
}

ErrorType::~ErrorType()
{
}

AbstractType::Ptr ErrorType::baseType() const
{
    return d_func()->m_baseType.abstractType();
}

AbstractType::Ptr ErrorType::errorType() const
{
    return d_func()->m_errorType.abstractType();
}

void ErrorType::setBaseType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_baseType = IndexedType(type);
}

void ErrorType::setErrorType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_errorType = IndexedType(type);
}

QString ErrorType::toString() const
{
    QString baseString = (baseType() ? baseType()->toString() : QStringLiteral("<notype>"));
    QString errorString = (errorType() ? errorType()->toString() : QStringLiteral(""));
    return errorString + QLatin1Char('!') + baseString + AbstractType::toString(true);
}

AbstractType::WhichType ErrorType::whichType() const
{
    return TypeUnsure;
}

uint ErrorType::hash() const
{
    return KDevHash(AbstractType::hash())
        << d_func()->m_baseType.hash()
        << d_func()->m_errorType.hash()
        << ComptimeType::hash();
}
}


