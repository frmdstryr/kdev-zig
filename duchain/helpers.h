/*
    SPDX-FileCopyrightText: 2011-2012 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/topducontext.h>

#include "kdevzigduchain_export.h"

namespace Zig
{

class KDEVZIGDUCHAIN_EXPORT Helper {
public:
    static KDevelop::Declaration* declarationForName(
        const QString& name,
        const KDevelop::CursorInRevision& location,
        KDevelop::DUChainPointer<const KDevelop::DUContext> context
    );

};

}
