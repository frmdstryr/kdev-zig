/*
 * Copyright 2017  Emma Gospodinova <emma.gospodinova@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZIGDUCONTEXT_H
#define ZIGDUCONTEXT_H

#include <language/duchain/duchainregister.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/topducontext.h>

#include "kdevzigduchain_export.h"

namespace Zig
{

using namespace KDevelop;

template<class BaseContext, int IdentityT>
class KDEVZIGDUCHAIN_EXPORT ZigDUContext : public BaseContext
{
public:
    template<class Data>
    explicit ZigDUContext(Data& data) : BaseContext(data) {
    }

    ///Parameters will be reached to the base-class
    template<typename... Params>
    explicit ZigDUContext(Params... params) : BaseContext(params...) {
        static_cast<KDevelop::DUChainBase*>(this)->d_func_dynamic()->setClassId(this);
    }

    enum {
        Identity = IdentityT
    };
};

using ZigTopDUContext = ZigDUContext<KDevelop::TopDUContext, 152>;
using ZigNormalDUContext = ZigDUContext<KDevelop::DUContext, 153>;

}

#endif // ZIGDUCONTEXT_H
