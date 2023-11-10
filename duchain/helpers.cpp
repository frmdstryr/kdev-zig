/*
    SPDX-FileCopyrightText: 2011-2013 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "helpers.h"
#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <language/duchain/duchainlock.h>
#include <language/duchain/types/identifiedtype.h>
#include <language/backgroundparser/backgroundparser.h>
#include <interfaces/ilanguagecontroller.h>
#include <util/path.h>
#include "types/declarationtypes.h"
#include "types/pointertype.h"
#include "types/builtintype.h"
#include <QProcess>
#include <QRegularExpression>
#include <kconfiggroup.h>
#include "zigdebug.h"
#include <interfaces/iprojectcontroller.h>


namespace Zig
{

QMutex Helper::cacheMutex;
QMap<IProject*, QVector<QUrl>> Helper::cachedCustomIncludes;
QMap<IProject*, QVector<QUrl>> Helper::cachedSearchPaths;
QVector<QUrl> Helper::projectSearchPaths;


// FIXME: Is this actually per project?
QMutex Helper::projectPathLock;
bool Helper::projectPackagesLoaded;
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

Declaration* Helper::accessAttribute(
    const AbstractType::Ptr accessed,
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
        DUChainReadLocker lock;
        const bool isModule = s->modifiers() & ModuleModifier;
        if (auto ctx = s->internalContext(isModule ? nullptr : topContext)) {
            auto decls = ctx->findLocalDeclarations(
                attribute,
                CursorInRevision::invalid(),
                topContext,
                AbstractType::Ptr(),
                DUContext::DontSearchInParent
            );
            if (!decls.isEmpty()) {
                return decls.last();
            }
        }
    }

    if (auto s = accessed.dynamicCast<EnumerationType>()) {
        DUChainReadLocker lock;
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

Declaration* Helper::declarationForIdentifiedType(
        const KDevelop::AbstractType::Ptr type,
        const KDevelop::TopDUContext* topContext)
{
    DUChainReadLocker lock;
    if (auto *t = dynamic_cast<IdentifiedType*>(type.data())) {
        return t->declaration(topContext);
    }
    return nullptr;
}

static inline bool contextTypeIsFnOrClass(const DUContext* ctx)
{
    return (
        ctx->type() == DUContext::Function
        || ctx->type() == DUContext::Class // Also needed for Zig's nested structs
    );
}

/**
 * In Zig order does not matter in container bodies
 */
static inline bool canFindBeyondUse(const DUContext* ctx)
{
    return (
        ctx && ctx->owner() && (
            ctx->owner()->isFunctionDeclaration()
            || (ctx->owner()->kind() == Declaration::Kind::Type)
        )
    );
}

Declaration* Helper::declarationForName(
    const QString& name,
    const CursorInRevision& location,
    DUChainPointer<const DUContext> context)
{
    DUChainReadLocker lock;
    const DUContext* currentContext = context.data();
    bool findBeyondUse = canFindBeyondUse(currentContext);
    CursorInRevision findUntil = findBeyondUse ? currentContext->topContext()->range().end : location;
    auto identifier = KDevelop::Identifier(name);
    auto localDeclarations = context->findLocalDeclarations(
        identifier,
        findUntil,
        nullptr,
        AbstractType::Ptr(), DUContext::DontResolveAliases);
    if ( !localDeclarations.isEmpty() ) {
        return localDeclarations.last();
    }

    QList<Declaration*> declarations;
    bool findInNext = true;
    do {
        if (findInNext) {
            findUntil = findBeyondUse ? currentContext->topContext()->range().end : location;
            declarations = currentContext->findDeclarations(identifier, findUntil);

            for (Declaration* declaration: declarations) {
                //qCDebug(KDEV_ZIG) << "decl " << declaration->toString();
                if (declaration->context()->type() != DUContext::Class
                    || (
                        contextTypeIsFnOrClass(currentContext)
                        //&& declaration->context() == currentContext->parentContext()
                    )
                ) {
                     // Declarations from struct decls must be referenced through `self.<foo>`, except
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

        if (!findBeyondUse && canFindBeyondUse(currentContext)) {
            // Names in the body may be defined after the function definition, before the function is called.
            // Note: only the parameter list has type DUContext::Function, so we have to do this instead.
            findBeyondUse = findInNext = true;
        }
    } while ((currentContext = currentContext->parentContext()));

    return nullptr;
}


KDevelop::TopDUContext* Helper::declarationTopContext(const KDevelop::Declaration* decl)
{
    if (!decl)
        return nullptr;

    if (auto alias = dynamic_cast<const AliasDeclaration*>(decl)) {
        auto d = alias->aliasedDeclaration();
        return d.indexedTopContext().data();
    }
    if (auto s = decl->abstractType().dynamicCast<StructureType>()) {
        if (s->modifiers() & ModuleModifier) {
            DUChainReadLocker lock;
            if (auto ctx = s->internalContext(nullptr)) {
                return ctx->topContext();
            }
        }
    }
    // if (decl->internalContext()) {
    //     return decl->internalContext()->topContext();
    // }
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
    qCWarning(KDEV_ZIG) << "zig exe not found. Using default";
    return "/usr/bin/zig";
}

void Helper::loadPackages(KDevelop::IProject* project)
{
    if (!project) {
        return;
    }

    QMutexLocker lock(&Helper::projectPathLock);

    // Keep old stdlib to avoid re-running zig
    QString oldStdLib = projectPackages.contains("std") ?
        *projectPackages.constFind("std"): "";

    projectPackages.clear();

    // Load packages

    QString pkgs = project->projectConfiguration()->group("kdevzigsupport").readEntry("zigPackages");
    qCDebug(KDEV_ZIG) << "zig packages configured" << pkgs;
    for (const QString &pkg : pkgs.split("\n")) {
        QStringList parts = pkg.split(":");
        if (parts.size() == 2) {
            QString pkgName = parts[0].trimmed();
            QString pkgPath = parts[1].trimmed();
            if (!pkgName.isEmpty() && !pkgPath.isEmpty()) {
                QString finalPath = QDir::isAbsolutePath(pkgPath) ? pkgPath :
                    QDir(project->path().toLocalFile()).filePath(pkgPath);
                qCDebug(KDEV_ZIG) << "zig package set: " << pkgName << ": " << finalPath;
                projectPackages.insert(pkgName, finalPath);
            }
        } else {
            qCDebug(KDEV_ZIG) << "zig package is invalid format: " << pkg;
        }
    }

    // If none is defined use the last saved
    if (!oldStdLib.isEmpty() && !projectPackages.contains("std")) {
        projectPackages.insert("std", oldStdLib);
    }

    projectPackagesLoaded = true;
}

QString Helper::stdLibPath(IProject* project)
{
    static QRegularExpression stdDirPattern("\\s*\"std_dir\":\\s*\"(.+)\"");
    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }
    // Use one from project config or previously loaded from zig
    if (projectPackages.contains("std")) {
        QDir stdzig = *projectPackages.constFind("std");
        stdzig.cdUp();
        return stdzig.path();
    }
    QProcess zig;
    QStringList args = {"env"};
    QString zigExe = zigExecutablePath(project);
    qCDebug(KDEV_ZIG) << "zig exe" << zigExe;
    if (QFile(zigExe).exists()) {
        zig.start(zigExe, args);
        zig.waitForFinished(1000);
        QString output = QString::fromUtf8(zig.readAllStandardOutput());
        qCDebug(KDEV_ZIG) << "zig env output:" << output;
        QStringList lines = output.split("\n");
        for (const QString &line: lines) {
            auto r = stdDirPattern.match(line);
            if (r.hasMatch()) {
                QString match = r.captured(1);
                QString path = QDir::isAbsolutePath(match) ? match :
                    QDir::home().filePath(match);
                qCDebug(KDEV_ZIG) << "std_lib" << path;
                projectPackages.insert("std", QDir::cleanPath(path + "/std.zig"));
                QDir stdzig = *projectPackages.constFind("std");
                stdzig.cdUp();
                return stdzig.path();
            }
        }
    }
    qCWarning(KDEV_ZIG) << "zig std lib path not found";
    return "/usr/local/lib/zig/lib/zig/std";
}

QString Helper::packagePath(const QString &name, const QString& currentFile)
{
    auto project = ICore::self()->projectController()->findProjectForUrl(
            QUrl::fromLocalFile(currentFile));

    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }
    if (projectPackages.contains(name)) {
        return *projectPackages.constFind(name);
    }
    if (name == "std") {
        lock.unlock();
        return QDir::cleanPath(stdLibPath(project) + "/std.zig");
    }
    qCDebug(KDEV_ZIG) << "No zig package path found for " << name;
    return "";
}

QUrl Helper::importPath(const QString& importName, const QString& currentFile)
{
    QUrl importPath;
    if (importName.endsWith(".zig")) {
        QDir folder = currentFile;
        folder.cdUp();
        importPath = QUrl::fromLocalFile(
            QDir::cleanPath(folder.absoluteFilePath(importName))
        );
    } else {
        importPath = QUrl::fromLocalFile(packagePath(importName, currentFile));
    }
    if (QFile(importPath.toLocalFile()).exists()) {
        return importPath;
    }
    qCDebug(KDEV_ZIG) << "@import(" << importName << ") does not exist" << importPath.toLocalFile();
    return QUrl("");
}

QString Helper::qualifierPath(const QString& currentFile)
{
    QString f = currentFile.endsWith(".zig") ? currentFile.mid(0, currentFile.size()-3) : currentFile;
    if (QDir::isRelativePath(f)) {
        return "";
    }
    auto project = ICore::self()->projectController()->findProjectForUrl(
            QUrl::fromLocalFile(currentFile));

    QMutexLocker lock(&Helper::projectPathLock);
    if (!projectPackagesLoaded) {
        lock.unlock();
        loadPackages(project);
        lock.relock();
    }

    for (const auto& pkgName: projectPackages.keys()) {
        const auto& pkgPath = projectPackages[pkgName];
        if (currentFile == pkgPath) {
            return pkgName;
        }
        QFileInfo pkg(pkgPath);
        QDir pkgRoot(pkg.absolutePath());
        QString relPath = pkgRoot.relativeFilePath(f);
        if (relPath.isEmpty() || relPath == '.') {
            return pkgName + '.';
        }
        if (!relPath.startsWith("..")) {
            return pkgName + '.' + relPath.replace(QDir::separator(), '.');
        }
    }
    if (project) {
        QDir projectRoot(project->path().toLocalFile());
        QString relPath = projectRoot.relativeFilePath(f);
        return relPath.replace(QDir::separator(), '.');
    }
    return "";
}

} // namespace zig
