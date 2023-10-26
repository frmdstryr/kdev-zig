#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include <QString>

namespace Zig
{

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

    char m_dataType[16];
};

class KDEVPLATFORMLANGUAGE_EXPORT BuiltinType : public AbstractType
{
public:
    using Ptr = TypePtr<BuiltinType>;

    BuiltinType(QString name);
    BuiltinType(const BuiltinType &rhs);
    explicit BuiltinType(BuiltinTypeData& data);
    ~BuiltinType();

    QString toString() const override;
    bool equals(const AbstractType* rhs) const override;
    uint hash() const override;
    WhichType whichType() const override;
    AbstractType* clone() const override;

    QString dataType() const;
    void setDataType(QString &dataType);

    using Data = BuiltinTypeData;

    // Returns null if it is not a zig builtin type
    static BuiltinType* newFromName(const QString &name);

    enum {
        Identity = 153
    };

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(BuiltinType)
};

}
