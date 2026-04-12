// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
// Qt-Security score:significant

#include "qqmljslinterrenamedcomponents_p.h"

QT_BEGIN_NAMESPACE

namespace QQmlJS {

using namespace Qt::StringLiterals;

static QStringView baseNameOf(const QString &path)
{
    if (!path.endsWith(".qml"_L1))
        return {};

    QStringView result(path);
    result.chop(".qml"_L1.size());
    if (result.endsWith(".ui"_L1))
        result.chop(".ui"_L1.size());
    const qsizetype start = path.lastIndexOf("/"_L1);
    result.slice(start == -1 ? 0 : start + 1);
    return result;
}

void LinterRenamedComponents::handleRenamedType(const QQmlJSScope::ConstPtr &scope,
                                                const QStringView name,
                                                const QQmlJS::SourceLocation &location,
                                                QQmlJSLogger *logger) const
{
    if (scope.isNull())
        return;

    auto [start, end] = m_scopeToName->equal_range(scope);
    if (start == end || std::next(start) == end)
        return;

    const QString oldName = baseNameOf(scope->filePath()).toString();
    if (oldName != name)
        return;

    QStringList names(start, end);
    names.erase(std::remove(names.begin(), names.end(), oldName), names.end());
    std::sort(names.begin(), names.end());

    logger->log(
            "\"%2\" is explicitly renamed to \"%1\" via a qmldir entry or QT_QML_SOURCE_TYPENAME CMake property, use \"%1\" instead."_L1
                    .arg(names.join("\", \""_L1), name),
            qmlRenamedType, location);
}
} // namespace QQmlJS

QT_END_NAMESPACE
