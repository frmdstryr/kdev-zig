/*
    SPDX-FileCopyrightText: 2011-2013 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "helpers.h"
#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <language/duchain/duchainlock.h>
#include <language/backgroundparser/backgroundparser.h>
#include <interfaces/ilanguagecontroller.h>
#include "types/declarationtypes.h"
#include "types/pointertype.h"
#include "types/builtintype.h"
#include <QProcess>
#include <QRegularExpression>
#include <kconfiggroup.h>


namespace Zig
{

QMutex Helper::cacheMutex;
QMap<IProject*, QVector<QUrl>> Helper::cachedCustomIncludes;
QMap<IProject*, QVector<QUrl>> Helper::cachedSearchPaths;
QVector<QUrl> Helper::projectSearchPaths;


// FIXME: Is this actually per project?
QMutex Helper::projectPathLock;
QMap<QString, QString> Helper::projectPackages;



void Helper::scheduleDependency(const IndexedString& dependency, int betterThanPriority)
{
    BackgroundParser* bgparser = KDevelop::ICore::self()->languageController()->backgroundParser();
    bool needsReschedule = true;
    if ( bgparser->isQueued(dependency) ) {
        const auto priority= bgparser->priorityForDocument(dependency);
        if ( priority > betterThanPriority - 1 ) {
            bgparser->removeDocument(dependency);
        }
        else {
            needsReschedule = false;
        }
    }
    if ( needsReschedule ) {
        bgparser->addDocument(dependency, TopDUContext::ForceUpdate, betterThanPriority - 1,
                              nullptr, ParseJob::FullSequentialProcessing);
    }
}

Declaration* Helper::accessAttribute(const AbstractType::Ptr accessed,
                                     const KDevelop::IndexedIdentifier& attribute,
                                     const KDevelop::TopDUContext* topContext)
{
    if ( ! accessed || !topContext ) {
        return nullptr;
    }
    if (auto ptr = accessed.dynamicCast<Zig::PointerType>()) {
        // Zig automatically walks pointers
        return accessAttribute(ptr->baseType(), attribute, topContext);
    }

    if (auto s = accessed.dynamicCast<StructureType>()) {
        const auto isModule = s->modifiers() & ModuleModifier;
        // Lookup in the original declaration's module context
        const auto moduleContext = (isModule) ? s->declaration(nullptr)->topContext() : topContext;
        if (auto ctx = s->internalContext(moduleContext)) {
            auto decls = ctx->findDeclarations(
                attribute, CursorInRevision::invalid(),
                topContext,
                DUContext::DontSearchInParent
            );
            if (!decls.isEmpty()) {
                return decls.first();
            }
        }
    }

    if (auto s = accessed.dynamicCast<EnumerationType>()) {
        if (auto ctx = s->internalContext(topContext)) {
            auto decls = ctx->findDeclarations(
                attribute, CursorInRevision::invalid(),
                topContext,
                DUContext::DontSearchInParent
            );
            if (!decls.isEmpty()) {
                return decls.first();
            }
        }
    }

    return nullptr;
}


Declaration* Helper::declarationForName(
    const QString& name,
    const CursorInRevision& location,
    DUChainPointer<const DUContext> context)
{
    DUChainReadLocker lock;
    auto identifier = KDevelop::Identifier(name);
    auto localDeclarations = context->findLocalDeclarations(identifier, location, nullptr,
                                                            AbstractType::Ptr(), DUContext::DontResolveAliases);
    if ( !localDeclarations.isEmpty() ) {
        return localDeclarations.last();
    }

    QList<Declaration*> declarations;
    const DUContext* currentContext = context.data();
    bool findInNext = true, findBeyondUse = false;
    do {
        if (findInNext) {
            CursorInRevision findUntil = findBeyondUse ? currentContext->topContext()->range().end : location;
            declarations = currentContext->findDeclarations(identifier, findUntil);

            for (Declaration* declaration: declarations) {
                if (declaration->context()->type() != DUContext::Class ||
                    (currentContext->type() == DUContext::Function && declaration->context() == currentContext->parentContext())) {
                     // Declarations from class decls must be referenced through `self.<foo>`, except
                     //  in their local scope (handled above) or when used as default arguments for methods of the same class.
                     // Otherwise, we're done!
                    return declaration;
                }
            }
            if (!declarations.isEmpty()) {
                // If we found declarations but rejected all of them (i.e. didn't return), we need to keep searching.
                findInNext = true;
                declarations.clear();
            }
        }

        if (!findBeyondUse && currentContext->owner() && currentContext->owner()->isFunctionDeclaration()) {
            // Names in the body may be defined after the function definition, before the function is called.
            // Note: only the parameter list has type DUContext::Function, so we have to do this instead.
            findBeyondUse = findInNext = true;
        }
    } while ((currentContext = currentContext->parentContext()));

    return nullptr;
}


KDevelop::DUContext* Helper::thisContext(
        const KDevelop::CursorInRevision& location,
        const KDevelop::TopDUContext* topContext)
{
    if (!location.isValid()) {
        return nullptr;
    }
    DUContext* currentContext = topContext->findContextAt(location);
    while (currentContext) {
        auto contextType = currentContext->type();
        if (contextType == DUContext::Class
            || contextType == DUContext::Namespace
            || contextType == DUContext::Enum
        ) {
            return currentContext;
        }
        currentContext = currentContext->parentContext();
    }
    return nullptr;

}


QString Helper::zigExecutablePath(IProject* project)
{
    if ( project ) {
        auto zigExe = project->projectConfiguration()->group("kdevzigsupport").readEntry("zigExecutable");
        if ( !zigExe.isEmpty() ) {
            QFile f(zigExe);
            if ( f.exists() ) {
                return zigExe;
            }
        }
    }
    auto result = QStandardPaths::findExecutable("zig");
    if ( ! result.isEmpty() ) {
        return result;
    }
    qWarning() << "zig exe not found. Using default";
    return "/usr/bin/zig";
}


QString Helper::stdLibPath(IProject* project)
{
    static QRegularExpression stdDirPattern("\\s*\"std_dir\":\\s*\"(.+)\"");
    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackages.contains("std")) {
        QProcess zig;
        QStringList args = {"env"};
        QString zigExe = zigExecutablePath(project);
        if (QFile(zigExe).exists()) {
            zig.start(zigExe, args);
            zig.waitForFinished(1000);
            QString output = QString::fromUtf8(zig.readAllStandardOutput());
            QStringList lines = output.split("\n");
            for (const QString &line: lines) {
                auto r = stdDirPattern.match(line);
                if (r.hasMatch()) {
                    QString path = r.captured(1);
                    projectPackages.insert("std", QString::fromUtf8(path.toUtf8()));
                    return *projectPackages.constFind("std");
                }
            }
        }
        qWarning() << "zig std lib path not found";
        return "/usr/local/lib/zig/lib/zig/std";
    }
    return *projectPackages.constFind("std");
}

QUrl Helper::importPath(const QString& importName, const QString& currentFile)
{
    QUrl importPath;
    if (importName == "std") {
        importPath = QUrl::fromLocalFile(QDir::cleanPath(stdLibPath(nullptr) + "/std.zig"));
    } else {
        QDir folder = currentFile;
        folder.cdUp();
        importPath = QUrl::fromLocalFile(QDir::cleanPath(folder.absoluteFilePath(importName)));
    }

    // qDebug() << "Import " << importName << "path " << importPath.toLocalFile();
    if (!QFile(importPath.toLocalFile()).exists()) {
        return QUrl("");
    }
    return importPath;
}


} // namespace zig
