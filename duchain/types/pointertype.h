
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
#include <QString>

namespace Zig
{

using namespace KDevelop;

// If pointer is to an array use this to render [*]
const quint32 ArrayModifier = 1 << 15;

class KDEVPLATFORMLANGUAGE_EXPORT PointerTypeData
    : public AbstractTypeData
{
public:
    /// Constructor
    PointerTypeData();
    /// Copy constructor. \param rhs data to copy
    PointerTypeData(const PointerTypeData& rhs);
    ~PointerTypeData() = default;
    PointerTypeData& operator=(const PointerTypeData& rhs) = delete;
    /// Type of data if the value is not null
    IndexedType m_baseType;
};

/**
 * \short A type representing optional types.
 *
 * PointerType is used to represent types which hold a can be null.
 */
class KDEVPLATFORMLANGUAGE_EXPORT PointerType
    : public AbstractType
{
public:
    using Ptr = TypePtr<PointerType>;

    /// Default constructor
    PointerType ();
    /// Copy constructor. \param rhs type to copy
    PointerType(const PointerType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit PointerType(PointerTypeData& data);
    /// Destructor
    ~PointerType() override;

    PointerType& operator=(const PointerType& rhs) = delete;

    /**
     * Sets the base type of the pointer, ie. what type of data the pointer points to.
     *
     * \param type the base type.
     */
    void setBaseType(const AbstractType::Ptr& type);

    /**
     * Retrieve the base type of the pointer, ie. what type of data the pointer points to.
     *
     * \returns the base type.
     */
    AbstractType::Ptr baseType () const;

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    AbstractType* clone() const override;

    bool equals(const AbstractType* rhs) const override;

    void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 156
    };

    using Data = PointerTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(PointerType)
};


}
