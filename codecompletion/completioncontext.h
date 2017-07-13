#ifndef COMPLETIONCONTEXT_H
#define COMPLETIONCONTEXT_H

#include <QString>

#include <language/codecompletion/codecompletioncontext.h>

namespace Rust
{

class CompletionContext : public KDevelop::CodeCompletionContext
{
public:
    CompletionContext(KDevelop::DUContextPointer context,
                      const QString &contextText,
                      const QString &followingText,
                      const KDevelop::CursorInRevision &position,
                      int depth);

    QList<KDevelop::CompletionTreeItemPointer> completionItems(bool &abort, bool fullCompletion) override;

private:
    QString m_followingText;
};

}

#endif // COMPLETIONCONTEXT_H