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

#ifndef QMULTIOBJECTREGISTRYREF_P_H
#define QMULTIOBJECTREGISTRYREF_P_H

#include <QtQmlDesignSupport/qmultiobjectregistryref.h>
#include <QtQmlDesignSupport/private/qabstractobjectregistryref_p.h>

QT_BEGIN_NAMESPACE

class Q_QMLDESIGNSUPPORT_EXPORT QMultiObjectRegistryRefPrivate
    : public QAbstractObjectRegistryRefPrivate
{
    Q_DECLARE_PUBLIC(QMultiObjectRegistryRef)

public:
    explicit QMultiObjectRegistryRefPrivate(QQmlEngine *engine = nullptr);

    void handleObjectAdded(QObject *obj) override;
    void handleObjectRemoved(QObject *obj) override;
    void handleInitialObjects() override;

    static qsizetype objectsCount(QQmlListProperty<QObject> *l);
    static QObject *objectsAt(QQmlListProperty<QObject> *l, qsizetype i);

private:
    QList<QObject *> m_objects;
};

QT_END_NAMESPACE

#endif // QMULTIOBJECTREGISTRYREF_P_H
