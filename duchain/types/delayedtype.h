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

namespace Zig {

using ComptimeTypeBase = MergeComptimeType<AbstractType>;

class KDEVPLATFORMLANGUAGE_EXPORT DelayedTypeData
    : public ComptimeTypeBase::Data
{
public:
    /// Constructor
    DelayedTypeData() = default;
    /// Copy constructor. \param rhs data to copy
    DelayedTypeData(const DelayedTypeData& rhs);
    ~DelayedTypeData() = default;
    DelayedTypeData& operator=(const DelayedTypeData& rhs) = delete;
    // Name of the identifier
    KDevelop::IndexedString m_identifier;
};

/**
 * \short A type which has not yet been resolved.
 *
 * Delayed types can be used for any types that cannot be resolved in the moment they are encountered.
 * They can be used for example in template-classes, or to store the names of unresolved types.
 * In a template-class, many types can not be evaluated at the time they are used, because they depend on unknown template-parameters.
 * Delayed types store the way the type would be searched, and can be used to find the type once the template-paremeters have values.
 *
 * The builtin delayed type always crashes due to broken reference counting
 * this uses indexed string for the identifier instead.
 * */
class KDEVPLATFORMLANGUAGE_EXPORT DelayedType
    : public ComptimeTypeBase
{
public:
    using Ptr = TypePtr<DelayedType>;

    /// Default constructor
    DelayedType();
    /// Copy constructor. \param rhs type to copy
    DelayedType(const DelayedType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit DelayedType(DelayedTypeData& data);
    /// Destructor
    ~DelayedType() override = default;

    DelayedType& operator=(const DelayedType& rhs) = delete;

    /**
     * Access the type identifier which this type represents.
     *
     * \returns the type identifier.
     */
    KDevelop::IndexedString identifier() const;

    /**
     * Set the type identifier which this type represents.
     *
     * \param identifier the type identifier.
     */
    void setIdentifier(const KDevelop::IndexedString& identifier);
    inline void setIdentifier(const QString& identifier)
    {
        setIdentifier(IndexedString(identifier));
    }

    QString toString() const override;

    KDevelop::AbstractType* clone() const override;

    bool equalsIgnoringValue(const KDevelop::AbstractType* rhs) const override;

    uint hash() const override;

    WhichType whichType() const override;

    enum {
        Identity = 160
    };

    using Data = DelayedTypeData;

protected:
    void accept0 (KDevelop::TypeVisitor* v) const override;
    TYPE_DECLARE_DATA(DelayedType)
};


} // namespace zig
