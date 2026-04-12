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

#ifndef QOBJECTREGISTRYSINGLETON_P_H
#define QOBJECTREGISTRYSINGLETON_P_H

#include <QtQmlDesignSupport/qtqmldesignsupportexports.h>

#include <QtCore/qhash.h>
#include <QtCore/qobject.h>
#include <QtCore/qset.h>
#include <QtCore/qstring.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QAbstractObjectRegistryRefPrivate;
class QQmlEngine;

class Q_QMLDESIGNSUPPORT_EXPORT QObjectRegistrySingleton : public QObject
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(InternalObjectRegistry)

public:
    explicit QObjectRegistrySingleton(QObject *parent = nullptr);

    void registerRef(QAbstractObjectRegistryRefPrivate *ref);
    void deregisterRef(QAbstractObjectRegistryRefPrivate *ref);

    void add(const QString &key, QObject *obj);
    void remove(const QString &key, QObject *obj);
    QSet<QObject*> objects(const QString &key) const;

    static QObjectRegistrySingleton *registryForObject(QObject *obj);
    static QObjectRegistrySingleton *registryForEngine(QQmlEngine *engine);

private:
    QHash<QString, QSet<QObject *>> m_objects;
    QHash<QString, QSet<QAbstractObjectRegistryRefPrivate *>> m_refs;
};

QT_END_NAMESPACE

#endif // QOBJECTREGISTRYSINGLETON_P_H
