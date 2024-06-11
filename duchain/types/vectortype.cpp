#include "vectortype.h"
#include "slicetype.h"

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/integraltype.h"
#include "language/duchain/types/structuretype.h"

#include "language/duchain/types/arraytype.h"
#include <helpers.h>

namespace Zig {

using namespace KDevelop;

VectorTypeData::VectorTypeData(const VectorTypeData& rhs)
    : ComptimeTypeBase::Data(rhs)
    , m_dimension(rhs.m_dimension)
    , m_elementType(rhs.m_elementType)
{
}

REGISTER_TYPE(VectorType);

VectorType::VectorType(const VectorType& rhs)
    : ComptimeTypeBase(copyData<VectorType>(*rhs.d_func()))
{
}

VectorType::VectorType(VectorTypeData& data)
    : ComptimeTypeBase(data)
{
}

VectorType::VectorType()
    : ComptimeTypeBase(createData<VectorType>())
{
}

AbstractType* VectorType::clone() const
{
    return new VectorType(*this);
}

bool VectorType::equalsIgnoringValueAndDimension(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;
    if (!ComptimeTypeBase::equalsIgnoringValue(_rhs))
        return false;
    Q_ASSERT(dynamic_cast<const VectorType*>(_rhs));
    const auto* rhs = static_cast<const VectorType*>(_rhs);
    TYPE_D(VectorType);
    return d->m_elementType == rhs->d_func()->m_elementType;

}

bool VectorType::equalsIgnoringValue(const AbstractType* _rhs) const
{
    if (!equalsIgnoringValueAndDimension(_rhs))
        return false;
    const auto* rhs = static_cast<const VectorType*>(_rhs);
    // Check dimension
    return d_func()->m_dimension == rhs->d_func()->m_dimension;
}


int VectorType::dimension() const
{
    return d_func()->m_dimension;
}

void VectorType::setDimension(int dimension)
{
    d_func_dynamic()->m_dimension = dimension;
}

AbstractType::Ptr VectorType::elementType() const
{
    return d_func()->m_elementType.abstractType();
}


void VectorType::setElementType(const AbstractType::Ptr& type)
{
    d_func_dynamic()->m_elementType = IndexedType(type);
}

QString VectorType::toString() const
{
    // TODO: Clean this up...
    const auto T = elementType();
    QString type = T ? T->toString() : QStringLiteral("<notype>");
    QString v = isComptimeKnown() ? QStringLiteral(" = \"%1\"").arg(comptimeKnownValue().str()) : QStringLiteral("");
    return AbstractType::toString() + QStringLiteral("@Vector(%1, %2)%3").arg(d_func()->m_dimension).arg(type).arg(v);
}

void VectorType::accept0(TypeVisitor* v) const
{
    if (v->visit(this)) {
        acceptType(d_func()->m_elementType.abstractType(), v);
    }

    // v->endVisit(reinterpret_cast<const ArrayType*>(this));
}

void VectorType::exchangeTypes(TypeExchanger* exchanger)
{
    const auto oldType = d_func()->m_elementType.abstractType();
    auto newType = exchanger->exchange(oldType);
    if (oldType.data() != newType.data()) {
        // TODO: Should it copy all modifiers?
        if (oldType->modifiers() & ConstModifier
            && !(newType->modifiers() & ConstModifier)
        ) {
            AbstractType::Ptr copy(newType->clone());
            copy->setModifiers(newType->modifiers() | ConstModifier);
            newType = copy;
        }
    }
    d_func_dynamic()->m_elementType = IndexedType(newType);
}

AbstractType::WhichType VectorType::whichType() const
{
    return TypeUnsure;
}

uint VectorType::hash() const
{
    return KDevHash(AbstractType::hash())
           << (elementType() ? elementType()->hash() : 0)
           << dimension() << ComptimeType::hash();
}

bool VectorType::canValueBeAssigned(const AbstractType::Ptr &rhs, const KDevelop::IProject* project)  const
{
    Q_UNUSED(project);
    if (equalsIgnoringValue(rhs.data()))
        return true;

    if (const auto slice = rhs.dynamicCast<SliceType>()) {
        if (slice->dimension() == dimension()) {
            return elementType()->equals(slice->elementType().data());
        }
    }

    return false;

}

} // End namespace

