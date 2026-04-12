// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
// Qt-Security score:significant

#ifndef QQMLJSLINTERRENAMEDCOMPONENTS_P_H
#define QQMLJSLINTERRENAMEDCOMPONENTS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include <private/qqmljslogger_p.h>
#include <private/qqmljssourcelocation_p.h>
#include <private/qqmljsscope_p.h>

#include <QtCore/qhash.h>
#include <QtCore/qstring.h>
#include <QtCore/qvarlengtharray.h>

QT_BEGIN_NAMESPACE

namespace QQmlJS {

class LinterRenamedComponents
{
public:
    void handleRenamedType(const QQmlJSScope::ConstPtr &scope, const QStringView name,
                           const QQmlJS::SourceLocation &location, QQmlJSLogger *logger) const;

    void setScopeToName(const QMultiHash<QQmlJSScope::ConstPtr, QString> *scopeToName)
    {
        m_scopeToName = scopeToName;
    }

private:
    const QMultiHash<QQmlJSScope::ConstPtr, QString> *m_scopeToName = nullptr;
};

} // namespace QQmlJS

QT_END_NAMESPACE

#endif // QQMLJSLINTERRENAMEDCOMPONENTS_P_H
