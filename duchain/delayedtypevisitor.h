#pragma once
#include <language/duchain/types/typesystem.h>
#include "types/delayedtype.h"
#include "kdevzigduchain_export.h"

namespace Zig
{

/**
 * Find all delayed types
 */
class KDEVZIGDUCHAIN_EXPORT DelayedTypeFinder : public KDevelop::SimpleTypeVisitor
{
public:
    ~DelayedTypeFinder() = default;

    bool visit(const KDevelop::AbstractType* t) override {
        if (auto d = dynamic_cast<const Zig::DelayedType*>(t)) {
            // Must use a clone or it deletes the underlying types when the list
            // is destroyed
            delayedTypes.append(
                DelayedType::Ptr(static_cast<DelayedType*>(d->clone())));
            return false;
        }
        return true;
    }

    QList<DelayedType::Ptr> delayedTypes;
};


} // end namespace
