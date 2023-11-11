/*
    SPDX-FileCopyrightText: 2006 Roberto Raggi <roberto@kdevelop.org>
    SPDX-FileCopyrightText: 2006-2008 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007-2008 David Nolden <david.nolden.kdevelop@art-master.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "language/duchain/types/typesystemdata.h"
#include "language/duchain/types/typeregister.h"
#include "language/duchain/types/typesystem.h"
#include "language/duchain/topducontext.h"

#include "comptimetype.h"
#include "../kdevzigastparser.h"
#include <language/duchain/declaration.h>
#include <klocalizedstring.h>
#include "../parsesession.h"

namespace Zig {

using namespace KDevelop;

ComptimeTypeData::ComptimeTypeData(const ComptimeTypeData& rhs)
    : AbstractTypeData(rhs)
    , m_node(rhs.m_node)
    , m_declaration(rhs.m_declaration)
{
}

REGISTER_TYPE(ComptimeType);

ComptimeType::ComptimeType(NodeIndex node, const IndexedDeclaration& decl)
    : AbstractType(createData<ComptimeType>())
{
    setNode(node);
    setValueDeclaration(decl);
}

ComptimeType::ComptimeType(const ComptimeType& rhs)
    : AbstractType(copyData<ComptimeType>(*rhs.d_func()))
{
}

ComptimeType::ComptimeType(ComptimeTypeData& data)
    : AbstractType(data)
{
}

AbstractType* ComptimeType::clone() const
{
    return new ComptimeType(*this);
}

bool ComptimeType::equals(const AbstractType* _rhs) const
{
    if (this == _rhs)
        return true;

    if (!AbstractType::equals(_rhs))
        return false;

    Q_ASSERT(dynamic_cast<const ComptimeType*>(_rhs));
    const auto *rhs = static_cast<const ComptimeType*>(_rhs);
    if (d_func()->m_node != rhs->d_func()->m_node)
        return false;
    return d_func()->m_declaration == rhs->d_func()->m_declaration;
}

ZAst* ComptimeType::ast() const
{
    if (!valueDeclaration().isValid())
        return nullptr;
    const auto *topContext = valueDeclaration().indexedTopContext().data();
    if (!topContext)
        return nullptr;
    if (!topContext->ast())
        return nullptr;
    if (auto session = dynamic_cast<const ParseSessionData*>(topContext->ast().data())) {
        return session->ast();
    }
    return nullptr;
}



NodeIndex ComptimeType::node() const
{
    return d_func()->m_node;
}

void ComptimeType::setNode(NodeIndex node)
{
    d_func_dynamic()->m_node = node;
}

IndexedDeclaration ComptimeType::valueDeclaration() const
{
    return d_func()->m_declaration;
}

void ComptimeType::setValueDeclaration(const IndexedDeclaration& decl)
{
    d_func_dynamic()->m_declaration = decl;
}

QString ComptimeType::toString() const
{
    if (valueDeclaration().isValid()) {
        return i18n(
            "type of %1",
            valueDeclaration().declaration()->qualifiedIdentifier().toString()
        );
    }

    return QLatin1String("<type>") + AbstractType::toString(true);
}

void ComptimeType::accept0(TypeVisitor* v) const
{
    v->visit(this);
}

AbstractType::WhichType ComptimeType::whichType() const
{
    return TypeUnsure;
}

uint ComptimeType::hash() const
{
    return KDevHash(AbstractType::hash()) << node() << valueDeclaration();
}


}

