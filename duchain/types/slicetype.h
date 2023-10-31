/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    Literal copy / rename that updates to string
    // TODO: Can it just subclass ArrayType?

    SPDX-License-Identifier: LGPL-2.0-only
*/
#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "kdevzigduchain_export.h"

namespace Zig {

using namespace KDevelop;

/// Private data structure for SliceType
class KDEVZIGDUCHAIN_EXPORT SliceTypeData
    : public AbstractTypeData
{
public:
    /// Constructor
    SliceTypeData() = default;
    /// Copy constructor. \param rhs data to copy
    SliceTypeData(const SliceTypeData& rhs);
    ~SliceTypeData() = default;
    SliceTypeData& operator=(const SliceTypeData& rhs) = delete;
    /// Dimension of the array
    int m_dimension = 0;
    int m_sentinel = -1;
    /// Element type of the array
    IndexedType m_elementType;
};


class KDEVZIGDUCHAIN_EXPORT SliceType
    : public AbstractType
{
public:
    using Ptr = TypePtr<SliceType>;

    /// Default constructor
    SliceType();
    /// Copy constructor. \param rhs type to copy
    SliceType(const SliceType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit SliceType(SliceTypeData& data);
    /// Destructor
    ~SliceType() override;

    SliceType& operator=(const SliceType& rhs) = delete;

    AbstractType* clone() const override;

    bool equals(const AbstractType* rhs) const override;

    /**
     * Retrieve the dimension of this array type. Multiple-dimensioned
     * arrays will have another array type as their elementType().
     *
     * \returns the dimension of the array, or zero if the array is dimensionless (eg. int[])
     */
    int dimension () const;

    /**
     * Set this array type's dimension.
     * If @p dimension is zero, the array is considered dimensionless (eg. int[]).
     *
     * \param dimension new dimension, set to zero for a dimensionless type (eg. int[])
     */
    void setDimension(int dimension);

    /**
     * Retrieve the element type of the array, e.g. "int" for int[3].
     *
     * \returns the element type.
     */
    AbstractType::Ptr elementType () const;

    /**
     * Set the element type of the array, e.g. "int" for int[3].
     */
    void setElementType(const AbstractType::Ptr& type);

    // Only :0 is supported for now
    void setSentinel(int sentinel);
    int sentinel() const;

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    // void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 158
    };

    using Data = SliceTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(SliceType)
};
}
