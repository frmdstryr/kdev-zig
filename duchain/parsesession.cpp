/*
 * Copyright 2023  Jairus Martin <frmdstryr@protonmail.com>
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

#include "parsesession.h"

namespace Zig
{

ParseSessionData::ParseSessionData(const KDevelop::IndexedString &document,
                                   const QByteArray &contents,
                                   const KDevelop::ParseJob *job,
                                   int priority)
    : m_document(document)
      ,m_contents(contents)
      ,m_ast(nullptr)
      ,m_jobPriority(priority)
      ,m_job(job)
{
}

ParseSessionData::~ParseSessionData()
{
    if (m_ast != nullptr) {
        destroy_ast(m_ast);
        m_ast = nullptr;
    }
    m_job = nullptr;
}

void ParseSessionData::parse()
{
    m_ast = parse_ast(m_document.c_str(), m_contents.constData());
}

KDevelop::IndexedString ParseSession::languageString()
{
    static const KDevelop::IndexedString langString(QStringLiteral("Zig"));
    return langString;
}


ParseSession::ParseSession(const ParseSessionData::Ptr &data)
    : d(data)
{
}

ParseSession::~ParseSession()
{
}

void ParseSession::parse()
{
    clearUnresolvedImports();
    d->parse();
}

ParseSessionData::Ptr ParseSession::data() const
{
    return d;
}

void ParseSession::setData(const ParseSessionData::Ptr data)
{
    this->d = data;
}

KDevelop::IndexedString ParseSession::document() const
{
    return d->m_document;
}

ZAst *ParseSession::ast() const
{
    return d->m_ast;
}

void ParseSession::setPriority(int priority)
{
    d->m_jobPriority = priority;
}


int ParseSession::jobPriority() const
{
    return d->m_jobPriority;
}

const KDevelop::ParseJob* ParseSession::job() const
{
    return d->m_job;
}

void ParseSession::setContextOnNode(const ZigNode &node, KDevelop::DUContext *context)
{
    Q_ASSERT(node.ast == d->m_ast);
    d->m_nodeContextMap.insert(node.index, context);
}

KDevelop::DUContext *ParseSession::contextFromNode(const ZigNode &node)
{
    Q_ASSERT(node.ast == d->m_ast);
    return d->m_nodeContextMap.value(node.index);
}

void ParseSession::setTypeOnNode(const ZigNode &node, const KDevelop::AbstractType::Ptr &type)
{
    Q_ASSERT(node.ast == d->m_ast);
    d->m_nodeTypeMap.insert(node.index, type);
}

KDevelop::AbstractType::Ptr ParseSession::typeFromNode(const ZigNode &node)
{
    Q_ASSERT(node.ast == d->m_ast);
    return d->m_nodeTypeMap.value(node.index);
}

void ParseSession::setDeclOnNode(const ZigNode &node, const KDevelop::DeclarationPointer &decl)
{
    Q_ASSERT(node.ast == d->m_ast);
    d->m_nodeDeclMap.insert(node.index, decl);
}

KDevelop::DeclarationPointer ParseSession::declFromNode(const ZigNode &node)
{
    Q_ASSERT(node.ast == d->m_ast);
    return d->m_nodeDeclMap.value(node.index);
}

void ParseSession::addUnresolvedImport(const KDevelop::IndexedString &module)
{
    d->m_unresolvedImports.insert(module);
}

void ParseSession::clearUnresolvedImports()
{
    d->m_unresolvedImports.clear();
}

QSet<KDevelop::IndexedString> ParseSession::unresolvedImports() const
{
    return d->m_unresolvedImports;
}

}
