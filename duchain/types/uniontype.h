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
#include "language/duchain/types/structuretype.h"
#include "comptimetype.h"

namespace Zig
{

using namespace KDevelop;
using UnionTypeBase = MergeComptimeType<StructureType>;


class KDEVPLATFORMLANGUAGE_EXPORT UnionTypeData
    : public UnionTypeBase::Data
{
public:
    /// Constructor
    UnionTypeData() = default;
    /// Copy constructor. \param rhs data to copy
    UnionTypeData(const UnionTypeData& rhs);
    ~UnionTypeData() = default;
    UnionTypeData& operator=(const UnionTypeData& rhs) = delete;
    // Either a reference to the parent union
    // or the union tag type if
    IndexedType m_baseType;
    // Data type of the field
    IndexedType m_dataType;
};

/**
 * \short A type representing an enum value.
 *
 * UnionType can be comptime known. The parent must be an UnionType or null
 */
class KDEVPLATFORMLANGUAGE_EXPORT UnionType
    : public UnionTypeBase
{
public:
    using Ptr = TypePtr<UnionType>;

    /// Default constructor
    UnionType();
    /// Copy constructor. \param rhs type to copy
    UnionType(const UnionType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit UnionType(UnionTypeData& data);
    /// Destructor
    ~UnionType() override;

    UnionType& operator=(const UnionType& rhs) = delete;

    /**
     * Sets the underlying type of the enum if it's a tagged union.
     *
     * \param type the enum type.
     */
    void setBaseType(const AbstractType::Ptr& type);

    /**
     * Retrieve the underlying enumeration data type.
     * WARNING: May be nullptr.
     */
    AbstractType::Ptr baseType() const;

    /**
     * Sets the data type of a union field
     */
    void setDataType(const AbstractType::Ptr& type);

    /**
     * Retrieve union value type.
     * WARNING: May be nullptr.
     */
    AbstractType::Ptr dataType() const;

    // Enum if it can be used in switches.
    // May be null or may be a union in the case of union(enum)
    AbstractType::Ptr enumType() const;

    inline bool isEnum() const
    {
        return enumType().data();
    }

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    AbstractType* clone() const override;

    bool equalsIgnoringValue(const AbstractType* rhs) const override;
    bool canValueBeAssigned(const AbstractType::Ptr &rhs) const override;

    void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 161
    };

    using Data = UnionTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(UnionType)
};


} // endnamespace


