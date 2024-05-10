
/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    // Literally a copy of the builitin Pointer type that
    // updates the toString

    SPDX-License-Identifier: LGPL-2.0-only
*/
#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/enumerationtype.h"
#include "comptimetype.h"

namespace Zig
{

using namespace KDevelop;
using EnumTypeBase = MergeComptimeType<EnumerationType>;
const quint32 ErrorSetModifier = 1 << 17;


class KDEVPLATFORMLANGUAGE_EXPORT EnumTypeData
    : public EnumTypeBase::Data
{
public:
    /// Constructor
    EnumTypeData() = default;
    /// Copy constructor. \param rhs data to copy
    EnumTypeData(const EnumTypeData& rhs);
    ~EnumTypeData() = default;
    EnumTypeData& operator=(const EnumTypeData& rhs) = delete;
    // Either the parent enumeration type in case of an enum value
    // Or the builitin value if any
    IndexedType m_enumType;
};

/**
 * \short A type representing an enum value.
 *
 * EnumType can be comptime known. The parent must be an EnumType or null.
 */
class KDEVPLATFORMLANGUAGE_EXPORT EnumType
    : public EnumTypeBase
{
public:
    using Ptr = TypePtr<EnumType>;

    /// Default constructor
    EnumType();
    /// Copy constructor. \param rhs type to copy
    EnumType(const EnumType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit EnumType(EnumTypeData& data);
    /// Destructor
    ~EnumType() override;

    EnumType& operator=(const EnumType& rhs) = delete;

    /**
     * Sets the underlying type of the enum if integer backed.
     *
     * \param type the enum type.
     */
    void setEnumType(const AbstractType::Ptr& type);

    /**
     * Retrieve the underlying enumeration data type.
     * WARNING: May be nullptr.
     * If the enumType casts to another EnumType it is considered an enum value.
     * For example with `const Dir = enum {Fwd, Rev};`
     * The enumType() for "Dir" will be null. The enumType() for Fwd and Rev
     * will be the parent "Dir" EnumType. If Dir was defiend with a type
     * (ex enum(u8)), then the enumType for Dir will be the BuiltinType for u8.
     */
    AbstractType::Ptr enumType() const;

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    AbstractType* clone() const override;

    bool equalsIgnoringValue(const AbstractType* rhs) const override;
    bool canValueBeAssigned(const AbstractType::Ptr &rhs) const override;

    void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 159
    };

    using Data = EnumTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(EnumType)
};


}

