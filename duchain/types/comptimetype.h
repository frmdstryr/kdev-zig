/*
    SPDX-FileCopyrightText: 2002-2005 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006 Adam Treat <treat@kde.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>
    SPDX-FileCopyrightText: 2023 Jairus Martin <frmdstryr@protonmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "kdevplatform/serialization/indexedstring.h"
#include <QString>

namespace Zig
{

using namespace KDevelop;

// If value is comptime known
const quint32 ComptimeModifier = 1 << 14;

class ComptimeTypeData
{
public:
    IndexedString m_comptimeValue;
};



/**
 * \short A type representing comptime known types.
 *
 * ComptimeType is used to represent types which are comptime known.
 */
class KDEVPLATFORMLANGUAGE_EXPORT ComptimeType
{
public:
    virtual ~ComptimeType() = default;

    /**
     * Test for equivalence with another type.
     *
     * \param rhs other type to check for equivalence
     * \returns true if equal, otherwise false.
     */
    inline bool equals(const ComptimeType* rhs) const
    {
        return (comptimeData()->m_comptimeValue == rhs->comptimeData()->m_comptimeValue);

    }

    /// Clear the comptime data
    inline void clearComptimeValue()
    {
        comptimeData()->m_comptimeValue = IndexedString();
    }

    /**
     * Check if this type has a comptime known value
     */
    inline bool isComptimeKnown() const
    {
        return !comptimeKnownValue().isEmpty();
    }

    /**
     * Get the comptime known value string.
     */
    inline const IndexedString& comptimeKnownValue() const
    {
        return comptimeData()->m_comptimeValue;
    }

    /**
     * Set the comptime known value. The interpretation of which
     * depends on the parent type.
     */
    inline void setComptimeKnownValue(const IndexedString &value)
    {
        comptimeData()->m_comptimeValue = value;
    }

    inline void setComptimeKnownValue(const QString &value)
    {
        setComptimeKnownValue(IndexedString(value));
    }

    // Check if types are equal ignoring any comptime known value information
    virtual bool equalsIgnoringValue(const AbstractType* rhs) const = 0;

    // Check if the value can be assigned
    virtual bool canValueBeAssigned(const AbstractType::Ptr &rhs) const = 0;

    /// Allow ComptimeType to access its data.
    virtual ComptimeTypeData* comptimeData() = 0;
    /// Allow IdentifiedType to access its data.
    virtual const ComptimeTypeData* comptimeData() const = 0;

    /// Allow ComptimeType to cast back to self
    virtual AbstractType::Ptr asType() = 0;

    /// Determine this identified type's hash value. \returns the hash value
    inline uint hash() const
    {
        return comptimeData()->m_comptimeValue.hash();
    }
};


///Implements everything necessary to merge the given Parent class with ComptimeType
///Your used Data class must be based on the Data member class
template <class Parent>
class MergeComptimeType
    : public Parent
    , public ComptimeType
{
public:

    class Data
        : public Parent::Data
        , public ComptimeTypeData
    {
    };

    MergeComptimeType() = default;

    explicit MergeComptimeType(Data& data) : Parent(data)
    {
    }

    MergeComptimeType(const MergeComptimeType& rhs) = delete;

    inline ComptimeTypeData* comptimeData() override
    {
        return static_cast<Data*>(this->d_func_dynamic());
    }

    inline const ComptimeTypeData* comptimeData() const override
    {
        return static_cast<const Data*>(this->d_func());
    }

    // Any ComptimeType subclasses should reimplement this instead of equals
    bool equalsIgnoringValue(const KDevelop::AbstractType* rhs) const override
    {
        return Parent::equals(rhs);
    }

    bool equals(const KDevelop::AbstractType* rhs) const override
    {
        if (!equalsIgnoringValue(rhs))
            return false;

        const auto* rhsId = dynamic_cast<const ComptimeType*>(rhs);
        Q_ASSERT(rhsId);
        return ComptimeType::equals(static_cast<const ComptimeType*>(rhsId));
    }

    bool canValueBeAssigned(const AbstractType::Ptr &rhs) const override
    {
        return equalsIgnoringValue(rhs.data());
    }

    /// Allow ComptimeType to cast back to self
    AbstractType::Ptr asType() override
    {
        return AbstractType::Ptr(this);
    }
};

} // Namespace zig
