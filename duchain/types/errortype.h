/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include <QString>

namespace Zig
{

using namespace KDevelop;

class KDEVPLATFORMLANGUAGE_EXPORT ErrorTypeData
    : public AbstractTypeData
{
public:
    /// Constructor
    ErrorTypeData();
    /// Copy constructor. \param rhs data to copy
    ErrorTypeData(const ErrorTypeData& rhs);
    ~ErrorTypeData() = default;
    ErrorTypeData& operator=(const ErrorTypeData& rhs) = delete;
    /// Type of data if the value is not null
    IndexedType m_baseType;
    IndexedType m_errorType;
};

/**
 * \short A type representing optional types.
 *
 * ErrorType is used to represent types which hold a can be null.
 */
class KDEVPLATFORMLANGUAGE_EXPORT ErrorType
    : public AbstractType
{
public:
    using Ptr = TypePtr<ErrorType>;

    /// Default constructor
    ErrorType ();
    /// Copy constructor. \param rhs type to copy
    ErrorType(const ErrorType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit ErrorType(ErrorTypeData& data);
    /// Destructor
    ~ErrorType() override;

    ErrorType& operator=(const ErrorType& rhs) = delete;

    /**
     * Sets the base type of the value.
     *
     * \param type the base type.
     */
    void setBaseType(const AbstractType::Ptr& type);
    /**
     * Sets the error type of the value.
     *
     * \param type the base type.
     */
    void setErrorType(const AbstractType::Ptr& type);

    /**
     * Retrieve the base type of the value
     *
     * \returns the base type.
     */
    AbstractType::Ptr baseType () const;
    /**
     * Retrieve the error type of the value.
     *
     * \returns the base type.
     */
    AbstractType::Ptr errorType () const;

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    AbstractType* clone() const override;

    bool equals(const AbstractType* rhs) const override;

    // void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 157
    };

    using Data = ErrorTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(ErrorType)
};


}