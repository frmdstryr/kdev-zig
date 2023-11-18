#pragma once
#include <language/duchain/types/typesystem.h>
#include <language/duchain/types/delayedtype.h>
#include "kdevzigduchain_export.h"

namespace Zig
{

using namespace KDevelop;
/**
 * Find all delayed types
 */
class KDEVZIGDUCHAIN_EXPORT DelayedTypeFinder : public SimpleTypeVisitor
{
public:
    ~DelayedTypeFinder() = default;

    bool visit(const AbstractType* t) override {
        if (dynamic_cast<const DelayedType*>(t)) {
            auto d = static_cast<const DelayedType*>(t);
            if (!delayedTypes.contains(d))
                delayedTypes.append(d);
            return false;
        }
        return true;
    }

    QList<const DelayedType*> delayedTypes;
};


} // end namespace
