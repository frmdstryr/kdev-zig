#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "kdevplatform/serialization/indexedstring.h"
#include "kdevzigduchain_export.h"
#include "comptimetype.h"

namespace Zig {

using namespace KDevelop;
using ComptimeTypeBase = MergeComptimeType<AbstractType>;

/// Private data structure for VectorType
class KDEVZIGDUCHAIN_EXPORT VectorTypeData
    : public ComptimeTypeBase::Data
{
public:
    VectorTypeData() = default;
    VectorTypeData(const VectorTypeData& rhs);
    ~VectorTypeData() = default;
    VectorTypeData& operator=(const VectorTypeData& rhs) = delete;
    /// Dimension of the vector
    int m_dimension = 0;
    /// Element type of the vector
    IndexedType m_elementType;
};


class KDEVZIGDUCHAIN_EXPORT VectorType: public ComptimeTypeBase
{
public:
    using Ptr = TypePtr<VectorType>;

    /// Default constructor
    VectorType();
    /// Copy constructor. \param rhs type to copy
    VectorType(const VectorType& rhs);
    /// Constructor using raw data. \param data internal data.
    explicit VectorType(VectorTypeData& data);
    /// Destructor
    ~VectorType() override = default;

    VectorType& operator=(const VectorType& rhs) = delete;

    AbstractType* clone() const override;

    bool equalsIgnoringValue(const AbstractType* rhs) const override;
    bool equalsIgnoringValueAndDimension(const AbstractType* rhs) const;

    bool canValueBeAssigned(const AbstractType::Ptr &rhs) const override;

    /**
     * Retrieve the dimension of this vector type. eg "3" for "@Vector(3, f32)"
     *
     * \returns the dimension of the vector
     */
    int dimension () const;

    /**
     * Set this vector type's dimension.
     */
    void setDimension(int dimension);

    /**
     * Retrieve the element type of the array, e.g. "f32" for "@Vector(3, f32)".
     *
     * \returns the element type.
     */
    AbstractType::Ptr elementType () const;

    /**
     * Set the element type of the vector, e.g. "f32" for "@Vector(3, f32)".
     */
    void setElementType(const AbstractType::Ptr& type);

    QString toString() const override;

    uint hash() const override;

    WhichType whichType() const override;

    void exchangeTypes(TypeExchanger* exchanger) override;

    enum {
        Identity = 162
    };

    using Data = VectorTypeData;

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(VectorType)
};
}
