#pragma once

#include "language/duchain/types/abstracttype.h"
#include "language/duchain/types/typesystemdata.h"
#include <QString>
#include "kdevzigastparser.h"

namespace Zig
{

using namespace KDevelop;

class KDEVPLATFORMLANGUAGE_EXPORT ComptimeTypeData: public AbstractTypeData
{
public:
    ComptimeTypeData() = default;
    ComptimeTypeData(const ComptimeTypeData& rhs);
    ComptimeTypeData& operator=(const ComptimeTypeData& rhs) = delete;
    ~ComptimeTypeData() = default;
    // Node index in the ast which provides the value
    NodeIndex m_node = 0;
    IndexedDeclaration m_declaration;
};

class KDEVPLATFORMLANGUAGE_EXPORT ComptimeType : public AbstractType
{
public:
    using Ptr = TypePtr<ComptimeType>;
    /**
     * This is almost a clone of an TypeAliasType except it also holds the
     * AST node
     */
    ComptimeType(NodeIndex node, const IndexedDeclaration& decl);
    ComptimeType(const ComptimeType &rhs);
    explicit ComptimeType(ComptimeTypeData& data);
    ~ComptimeType() override = default;

    ComptimeType& operator=(const ComptimeType& rhs) = delete;

    QString toString() const override;
    bool equals(const AbstractType* rhs) const override;
    uint hash() const override;
    WhichType whichType() const override;
    AbstractType* clone() const override;

    // NOTE: Caller must hold read lock
    ZAst* ast() const;

    NodeIndex node() const;
    void setNode(NodeIndex node);

    IndexedDeclaration valueDeclaration() const;
    void setValueDeclaration(const IndexedDeclaration& decl);

    using Data = ComptimeTypeData;

    enum {
        Identity = 159
    };

protected:
    void accept0 (TypeVisitor* v) const override;

    TYPE_DECLARE_DATA(ComptimeType)

};

}

