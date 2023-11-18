#pragma once
#include <QString>
#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "comptimetype.h"
#include "kdevzigastparser.h"
#include "kdevzigduchain_export.h"

namespace Zig
{


// If struct is a module / file
const quint32 ModuleModifier = 1 << 15;

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

    uint hash() const override;
    WhichType whichType() const override;
    AbstractType* clone() const override;

    const IndexedString& dataType() const;
    void setDataType(const IndexedString &dataType);
    inline void setDataType(const QString &dataType) { setDataType(IndexedString(dataType)); }

    bool isSigned() const;
    bool isUnsigned() const;
    bool isFloat() const;
    bool isComptimeInt() const;
    bool isComptimeFloat() const;
    bool isInteger() const { return isSigned() || isUnsigned(); }
    bool isNumeric() const { return isInteger() || isFloat(); }
    bool isBool() const;

    // "null" (does not check nullptr)
    bool isNull() const;
    // "undefined"
    bool isUndefined() const;
    // "type"
    bool isType() const;
    // "anytype"
    bool isAnytype() const;
    // "void"
    bool isVoid() const;

    // Get bitsize of an int (-1 if not in or unkown type)
    int bitsize() const;

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
