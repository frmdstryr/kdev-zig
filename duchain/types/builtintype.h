#pragma once
#include <QString>
#include <interfaces/iproject.h>
#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "comptimetype.h"
#include "kdevzigastparser.h"
#include "kdevzigduchain_export.h"

namespace Zig
{


// If struct is a module / file
const quint32 ModuleModifier = 1 << 15;
const quint32 CIncludeModifier = 1 << 16;

using namespace KDevelop;
using ComptimeTypeBase = MergeComptimeType<AbstractType>;

class KDEVZIGDUCHAIN_EXPORT BuiltinTypeData
    : public ComptimeTypeBase::Data
{
public:
    BuiltinTypeData();
    BuiltinTypeData(const BuiltinTypeData& rhs);
    BuiltinTypeData& operator=(const BuiltinTypeData& rhs) = delete;
    ~BuiltinTypeData() = default;

    // The type name
    IndexedString m_data;
};

class KDEVZIGDUCHAIN_EXPORT BuiltinType: public ComptimeTypeBase
{
public:
    using Ptr = TypePtr<BuiltinType>;

    BuiltinType(const QString &name);
    BuiltinType(const IndexedString &name);
    BuiltinType(const BuiltinType &rhs);
    explicit BuiltinType(BuiltinTypeData& data);
    ~BuiltinType() override = default;

    BuiltinType& operator=(const BuiltinType& rhs) = delete;

    QString toString() const override;
    bool equalsIgnoringValue(const AbstractType* rhs) const override;
    bool canValueBeAssigned(const AbstractType::Ptr &rhs, const KDevelop::IProject* project = nullptr) const override;

    uint hash() const override;
    WhichType whichType() const override;
    AbstractType* clone() const override;

    const IndexedString& dataType() const;
    void setDataType(const IndexedString &dataType);
    inline void setDataType(const QString &dataType) { setDataType(IndexedString(dataType)); }

    bool isChar() const; // c_char or u8
    bool isSigned() const;
    bool isUnsigned() const;
    bool isFloat() const;
    bool isComptimeInt() const;
    bool isComptimeFloat() const;
    bool isInteger() const { return isSigned() || isUnsigned(); }
    bool isNumeric() const { return isInteger() || isFloat(); }
    bool isBool() const;

    bool isTrue() const;
    bool isFalse() const;

    // "null" (does not check nullptr)
    bool isNull() const;
    bool isUndefined() const;
    bool isType() const;
    bool isAnytype() const;
    bool isVoid() const;
    bool isAnyframe() const;
    bool isAnyerror() const;
    bool isFrame() const;
    bool isOpaque() const;
    bool isNoreturn() const;
    bool isTrap() const;
    bool isUnreachable() const;

    // Get bitsize of an int (-1 if not in or unkown type)
    // Since some times (eg isize/usize) depend on the build target the project
    // can be passed to specify which.
    int bitsize(const KDevelop::IProject* project = nullptr) const;

    using Data = BuiltinTypeData;

    // Returns null if it is not a zig builtin type
    static bool isBuiltinFunc(const QString &name);
    static bool isBuiltinType(const QString &name);
    static bool isBuiltinVariable(const QString &name);
    static AbstractType::Ptr newFromName(const QString &name);

    enum {
        Identity = 154
    };

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(BuiltinType)

};

}
