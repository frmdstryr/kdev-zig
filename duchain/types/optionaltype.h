/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "comptimetype.h"

namespace Zig
{

using namespace KDevelop;
using ComptimeTypeBase = MergeComptimeType<AbstractType>;

class KDEVPLATFORMLANGUAGE_EXPORT OptionalTypeData
    : public ComptimeTypeBase::Data
{
public:
    /// Constructor
    OptionalTypeData();
    /// Copy constructor. \param rhs data to copy
    OptionalTypeData(const OptionalTypeData& rhs);
    ~OptionalTypeData() = default;
    OptionalTypeData& operator=(const OptionalTypeData& rhs) = delete;
    /// Type of data if the value is not null
    IndexedType m_baseType;
};

/**
 * \short A type representing optional types.
 *
 * OptionalType is used to represent types which hold a can be null.
 */
class KDEVPLATFORMLANGUAGE_EXPORT OptionalType
    : public ComptimeTypeBase
{
public:
    using Ptr = TypePtr<OptionalType>;

    /// Default constructor
    OptionalType ();
    /// Copy constructor. \param rhs type to copy
    OptionalType(const OptionalType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit OptionalType(OptionalTypeData& data);
    /// Destructor
    ~OptionalType() override;

    OptionalType& operator=(const OptionalType& rhs) = delete;

    /**
     * Sets the base type of the of the optional value
     *
     * \param type the base type.
     */
    void setBaseType(const AbstractType::Ptr& type);

    /**
     * Retrieve the base type of the optional value
     *
     * \returns the base type.
     */
    AbstractType::Ptr baseType () const;

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    AbstractType* clone() const override;

    bool equalsIgnoringValue(const AbstractType* rhs) const override;
    bool canValueBeAssigned(const AbstractType::Ptr &rhs, const KDevelop::IProject* project = nullptr) const override;
    void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 155
    };

    using Data = OptionalTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(OptionalType)
};


}
