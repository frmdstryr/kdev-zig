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

#include "zighighlighting.h"
#include <language/duchain/declaration.h>

using namespace KDevelop;

namespace Zig
{

class HighlightingInstance : public CodeHighlightingInstance
{
public:
    HighlightingInstance(const Highlighting *highlighting);
    ~HighlightingInstance() override;

    bool useRainbowColor(Declaration *dec) const override;
};

HighlightingInstance::HighlightingInstance(const Highlighting *highlighting)
    : CodeHighlightingInstance(highlighting)
{
}

HighlightingInstance::~HighlightingInstance()
{
}

bool HighlightingInstance::useRainbowColor(Declaration *dec) const
{
    return dec->context()->type() == DUContext::Function || dec->context()->type() == DUContext::Other;
}

Highlighting::Highlighting(QObject *parent)
    : CodeHighlighting(parent)
{
}

CodeHighlightingInstance *Highlighting::createInstance() const
{
    return new HighlightingInstance(this);
}

}
