// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef QOBJECTREGISTRYREF_P_H
#define QOBJECTREGISTRYREF_P_H

#include <QtQmlDesignSupport/qobjectregistryref.h>
#include <QtQmlDesignSupport/private/qabstractobjectregistryref_p.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;

class Q_QMLDESIGNSUPPORT_EXPORT QObjectRegistryRefPrivate : public QAbstractObjectRegistryRefPrivate
{
    Q_DECLARE_PUBLIC(QObjectRegistryRef)

public:
    explicit QObjectRegistryRefPrivate(QQmlEngine *engine = nullptr);

    void setObject(QObject *obj);
    void printMultiWarning() const;

    void handleObjectAdded(QObject *obj) override;
    void handleObjectRemoved(QObject *obj) override;
    void handleInitialObjects() override;

private:
    QObject *m_object = nullptr;
};

QT_END_NAMESPACE

#endif // QOBJECTREGISTRYREF_P_H
