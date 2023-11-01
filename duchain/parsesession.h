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

#ifndef PARSESESSION_H
#define PARSESESSION_H

#include <QMap>

#include <serialization/indexedstring.h>
#include <language/duchain/ducontext.h>
#include <language/interfaces/iastcontainer.h>

#include "zignode.h"

#include "kdevzigduchain_export.h"

namespace Zig
{

class KDEVZIGDUCHAIN_EXPORT ParseSessionData : public KDevelop::IAstContainer
{
public:
    typedef QExplicitlySharedDataPointer<ParseSessionData> Ptr;

    ParseSessionData(const KDevelop::IndexedString &document,
                     const QByteArray &contents,
                     int priority = 0);

    ~ParseSessionData() override;

private:
    friend class ParseSession;

    void parse();

    KDevelop::IndexedString m_document;
    QByteArray m_contents;
    ZAst *m_ast;
    int m_jobPriority;
    QMap<uint32_t, KDevelop::DUContext *> m_nodeContextMap;

};

class KDEVZIGDUCHAIN_EXPORT ParseSession
{
public:
    explicit ParseSession(const ParseSessionData::Ptr &data);
    ~ParseSession();

    void parse();

    ParseSessionData::Ptr data() const;
    void setData(const ParseSessionData::Ptr data);

    KDevelop::IndexedString document() const;
    ZAst *ast() const;
    void setPriority(int priority);
    int jobPriority() const;

    void setContextOnNode(const ZigNode &node, KDevelop::DUContext *context);
    KDevelop::DUContext *contextFromNode(const ZigNode &node);

private:
    Q_DISABLE_COPY(ParseSession)

    ParseSessionData::Ptr d;
};

}

#endif // PARSESESSION_H
