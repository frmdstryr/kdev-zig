#ifndef RUSTDUCONTEXT_H
#define RUSTDUCONTEXT_H

#include <language/duchain/duchainregister.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/topducontext.h>

#include "kdevrustduchain_export.h"

namespace Rust
{

using namespace KDevelop;

template<class BaseContext, int IdentityT>
class KDEVRUSTDUCHAIN_EXPORT RustDUContext : public BaseContext
{
public:
    template<class Data>
    explicit RustDUContext(Data& data) : BaseContext(data) {
    }

    ///Parameters will be reached to the base-class
    template<typename... Params>
    explicit RustDUContext(Params... params) : BaseContext(params...) {
        static_cast<KDevelop::DUChainBase*>(this)->d_func_dynamic()->setClassId(this);
    }

    enum {
        Identity = IdentityT
    };
};

using RustTopDUContext = RustDUContext<KDevelop::TopDUContext, 150>;
using RustNormalDUContext = RustDUContext<KDevelop::DUContext, 151>;

}

#endif // RUSTDUCONTEXT_H
