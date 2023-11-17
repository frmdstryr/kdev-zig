#pragma once
#include <QString>
#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include "kdevplatform/serialization/indexedstring.h"

#include "kdevzigastparser.h"

namespace Zig
{

// If value is comptime known
const quint32 ComptimeModifier = 1 << 14;
// If struct is a module / file
const quint32 ModuleModifier = 1 << 15;

using namespace KDevelop;

class KDEVPLATFORMLANGUAGE_EXPORT BuiltinTypeData
    : public AbstractTypeData
{
public:
    BuiltinTypeData();
    BuiltinTypeData(const QString &name);
    BuiltinTypeData(const BuiltinTypeData& rhs);
    BuiltinTypeData& operator=(const BuiltinTypeData& rhs) = delete;
    ~BuiltinTypeData() = default;

    void setData(const QString &name);
    // Either a type name or file
    IndexedString m_data;
    NodeIndex m_value = 0;
};

class KDEVPLATFORMLANGUAGE_EXPORT BuiltinType : public AbstractType
{
public:
    using Ptr = TypePtr<BuiltinType>;

    BuiltinType(QString name);
    BuiltinType(const BuiltinType &rhs);
    explicit BuiltinType(BuiltinTypeData& data);
    ~BuiltinType() override = default;

    BuiltinType& operator=(const BuiltinType& rhs) = delete;

    QString toString() const override;
    bool equals(const AbstractType* rhs) const override;
    uint hash() const override;
    WhichType whichType() const override;
    AbstractType* clone() const override;

    QString dataType() const;
    void setDataType(QString &dataType);

    NodeIndex valueNode() const;
    void setValueNode(NodeIndex node);
    bool isSigned() const;
    bool isUnsigned() const;
    bool isFloat() const;
    bool isComptimeInt() const;
    bool isComptimeFloat() const;
    bool isComptime() const { return isComptimeInt() || isComptimeFloat(); }
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
