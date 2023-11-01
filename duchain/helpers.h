/*
    SPDX-FileCopyrightText: 2011-2012 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <QMap>
#include <QVector>
#include <QUrl>
#include <interfaces/iproject.h>
#include <language/duchain/types/unsuretype.h>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/topducontext.h>

#include "kdevzigduchain_export.h"

namespace Zig
{

class KDEVZIGDUCHAIN_EXPORT Helper {
public:

    static QMutex cacheMutex;
    static QMap<KDevelop::IProject*, QVector<QUrl>> cachedCustomIncludes;
    static QMap<KDevelop::IProject*, QVector<QUrl>> cachedSearchPaths;
    static QMutex projectPathLock;
    static QVector<QUrl> projectSearchPaths;
    static QMap<QString, QString> projectPackages;

    static QString zigExecutablePath(KDevelop::IProject* project);
    static QString stdLibPath(KDevelop::IProject* project);
    static QUrl importPath(const QString& importName, const QString& currentFile);
    static QVector<QUrl> getSearchPaths(const QUrl& workingOnDocument);

    static void scheduleDependency(
        const KDevelop::IndexedString& dependency, int betterThanPriority);

    static KDevelop::Declaration* declarationForName(
        const QString& name,
        const KDevelop::CursorInRevision& location,
        KDevelop::DUChainPointer<const KDevelop::DUContext> context
    );

    /**
     * @brief Get the declaration of 'accessed.attribute', or return null.
     *
     * @param accessed Type (Structure or Unsure) that should have this attribute.
     * @param attribute Which attribute to look for.
     * @param topContext Top context (for this file?)
     * @return Declaration* of the attribute, or null.
     *  If UnsureType with >1 matching attributes, returns an arbitrary choice.
     **/
    static KDevelop::Declaration* accessAttribute(const KDevelop::AbstractType::Ptr accessed,
                                                  const KDevelop::IndexedIdentifier& attribute,
                                                  const KDevelop::TopDUContext* topContext);

    static KDevelop::Declaration* accessAttribute(const KDevelop::AbstractType::Ptr accessed,
        const QString& attribute, const KDevelop::TopDUContext* topContext) {
        return accessAttribute(accessed, KDevelop::IndexedIdentifier(KDevelop::Identifier(attribute)), topContext);
    }


    /**
     * Follows the context up to the closest container. Eg the builtin @This();
     * Return nullptr if no class or namespace parent context is found.
     * NOTE: Caller must hold at least a DUCHain read lock
     **/
    static KDevelop::DUContext* thisContext(
        const KDevelop::CursorInRevision& location,
        const KDevelop::TopDUContext* topContext);


};

}
