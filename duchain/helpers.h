/*
    SPDX-FileCopyrightText: 2011-2012 Sven Brauch <svenbrauch@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include <QUrl>
#include <interfaces/iproject.h>
#include <language/duchain/types/unsuretype.h>
#include <language/duchain/declaration.h>
#include <language/duchain/ducontext.h>
#include <language/duchain/topducontext.h>
#include <language/languageexport.h>

#include "kdevzigduchain_export.h"
#include "types/builtintype.h"
#include "types/slicetype.h"
#include "types/pointertype.h"

namespace Zig
{

class KDEVZIGDUCHAIN_EXPORT Helper {
public:

    static QMutex cacheMutex;
    static QMap<KDevelop::IProject*, QVector<QUrl>> cachedCustomIncludes;
    static QMap<KDevelop::IProject*, QVector<QUrl>> cachedSearchPaths;

    // The project... vars should only be accessed while holding the projectPathLock
    static QMutex projectPathLock;
    static QVector<QUrl> projectSearchPaths;
    static bool projectPackagesLoaded;
    static QMap<QString, QString> projectPackages;

    static QString zigExecutablePath(KDevelop::IProject* project);

    // Caller must NOT be holding projectPathLock for these
    static QString stdLibPath(KDevelop::IProject* project);
    static void loadPackages(KDevelop::IProject* project);
    // Lookup package root file from package name.
    // If name is std, returns <stdLibPath>/std.zig
    static QString packagePath(const QString &name, const QString& currentFile);
    // Lookup the path for an @import based relative to the current file
    // If the name is a package name, the packagePath is returned.
    static QUrl importPath(const QString& importName, const QString& currentFile);
    // Returns the qualifier for the given path. If the file is in one of the
    // package paths it will be relative to that. Eg std/fs.zig will return std.fs
    static QString qualifierPath(const QString& currentFile);
    // Lookup a cInclude path
    static QUrl includePath(const QString &name, const QString& currentFile);

    // Import a declaration based on the qualified name
    // eg "std.builtin.Type"
    static KDevelop::Declaration* declarationForImportedModuleName(
           const QString& module, const QString& currentFile);

    static QVector<QUrl> getSearchPaths(const QUrl& workingOnDocument);

    static void scheduleDependency(
        const KDevelop::IndexedString& dependency,
        int betterThanPriority,
        QObject* notifyWhenReady = nullptr);

    /**
     * Get the declaration for a name in the given context.
     * If excludedDeclaration is provided, do not use that one if found.
     **/
    static KDevelop::Declaration* declarationForName(
        const QString& name,
        const KDevelop::CursorInRevision& location,
        KDevelop::DUChainPointer<const KDevelop::DUContext> context,
        const KDevelop::Declaration* excludedDeclaration = nullptr
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
    static KDevelop::Declaration* accessAttribute(
        const KDevelop::AbstractType::Ptr& accessed,
        const KDevelop::IndexedIdentifier& attribute,
        const KDevelop::TopDUContext* topContext);

    static KDevelop::Declaration* accessAttribute(
        const KDevelop::AbstractType::Ptr& accessed,
        const QString& attribute,
        const KDevelop::TopDUContext* topContext
    )
    {
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

    /**
     * Shortcut that finds the declaration if the type casts to an Identified
     * type. May return null
     */
    static KDevelop::Declaration*  declarationForIdentifiedType(
        const KDevelop::AbstractType::Ptr type,
        const KDevelop::TopDUContext* topContext);

    /**
     * Check if types are equal factoring out any pointers.
     * So *Foo and Foo will be considered the same. If either types are null
     * or pointer base types are nullptr return false.
     */
    static bool baseTypesEqual(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b);

    /**
     * Check if types are equal if modifiers (eg const) are ignored
     */
    static bool typesEqualIgnoringModifiers(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b);

    /**
     * Check if types are equal ignoring any comptime known values
     */
    static bool typesEqualIgnoringComptimeValue(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b);

    /**
     * Check if the given value type can be assigned to the target
     * type without causing an error.
     */
    static bool canTypeBeAssigned(
        const KDevelop::AbstractType::Ptr &target,
        const KDevelop::AbstractType::Ptr &value);

    /**
     * If type is a pointer, return the base type, otherwise return type.
     */
    static const KDevelop::AbstractType::Ptr unwrapPointer(const KDevelop::AbstractType::Ptr &type);

    /**
     * Attempt to merge two types from an if condition.
     * If types are equal, return a
     * If one of the types is null return the other type (
     * coercing to optional as needed).
     * If one of the types is a comptime int/float, return the other
     * If one is optional and other is not optionl of same type, return the optional
     */
    static KDevelop::AbstractType::Ptr mergeTypes(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b,
        const KDevelop::DUContext* context);

    /**
     * If the type is comptime known clone and remove it.
     */
    static KDevelop::AbstractType::Ptr removeComptimeValue(
        const KDevelop::AbstractType::Ptr &a);

    /**
     * If types are not builtin return false
     */
    static bool canMergeNumericBuiltinTypes(
        const KDevelop::AbstractType::Ptr &a,
        const KDevelop::AbstractType::Ptr &b);

    /**
     * Check if type is an IntegralType with dataType of mixed. This
     * is the unknown type returned by the expression visitor.
     * @param checkPtr, if type is a pointer, check if base type is mixed
     */
    static bool isMixedType(const KDevelop::AbstractType::Ptr &a, bool checkPtr = true);

    /**
     * Check if the type has a comptime known value
     */
    static bool isComptimeKnown(const KDevelop::AbstractType::Ptr &a);

    /**
     * Attempt to evaulate an unsigned op of comptime known unsigned types.
     * It does not handle overflows
     */
    static AbstractType::Ptr evaluateUnsignedOp(
        const BuiltinType::Ptr &a,
        const BuiltinType::Ptr &b,
        const NodeTag &tag);
};

/**
 * A class which schedules the given url for reparsing when updateReady
 * is called. Intended to be used with Helper::scheduleDependency
 * TODO: This does not work.
 */
class KDEVZIGDUCHAIN_EXPORT ScheduleDependency
    : public QObject
{
    Q_OBJECT
public:
    ScheduleDependency(QObject* parent, const IndexedString& documentUrl, const IndexedString& dependencyUrl, int betterThanPriority = 0)
    : QObject(parent), m_documentUrl(documentUrl)
    {
        Helper::scheduleDependency(dependencyUrl, betterThanPriority, this);
    }
private Q_SLOTS:
    void updateReady(const IndexedString& url, const ReferencedTopDUContext& topContext);
private:
    const IndexedString &m_documentUrl;
};

}
