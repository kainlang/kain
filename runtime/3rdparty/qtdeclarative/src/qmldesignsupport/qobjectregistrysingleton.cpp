// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qobjectregistrysingleton_p.h"

#include <private/qabstractobjectregistryref_p.h>

#include <QtQml/qqmlengine.h>

QT_BEGIN_NAMESPACE

QObjectRegistrySingleton::QObjectRegistrySingleton(QObject *parent)
    : QObject(parent)
{
}

void QObjectRegistrySingleton::add(const QString &key, QObject *obj)
{
    if (key.isEmpty() || !obj)
        return;

    auto &objSet = m_objects[key];
    if (!objSet.contains(obj)) {
        objSet.insert(obj);
        const auto refs = m_refs.value(key);
        for (const auto &ref : refs)
            ref->handleObjectAdded(obj);
    }
}

void QObjectRegistrySingleton::remove(const QString &key, QObject *obj)
{
    if (key.isEmpty() || !obj)
        return;

    auto &objSet = m_objects[key];
    bool notifyListeners = objSet.remove(obj);
    if (objSet.isEmpty())
        m_objects.remove(key);
    if (notifyListeners) {
        const auto refs = m_refs.value(key);
        for (const auto &ref : refs)
            ref->handleObjectRemoved(obj);
    }
}

QSet<QObject*> QObjectRegistrySingleton::objects(const QString &key) const
{
    return m_objects.value(key);
}

void QObjectRegistrySingleton::registerRef(QAbstractObjectRegistryRefPrivate *ref)
{
    if (!ref)
        return;

    m_refs[ref->key()].insert(ref);
}

void QObjectRegistrySingleton::deregisterRef(QAbstractObjectRegistryRefPrivate *ref)
{
    if (!ref)
        return;

    if (m_refs.contains(ref->key())) {
        m_refs[ref->key()].remove(ref);
        if (m_refs[ref->key()].isEmpty())
            m_refs.remove(ref->key());
    }
}

QObjectRegistrySingleton *QObjectRegistrySingleton::registryForObject(QObject *obj)
{
    return registryForEngine(qmlEngine(obj));
}

QObjectRegistrySingleton *QObjectRegistrySingleton::registryForEngine(QQmlEngine *engine)
{
    if (engine) {
        static int typeId = qmlTypeId("QtQml.DesignSupport",
                                      QT_VERSION_MAJOR,
                                      QT_VERSION_MINOR,
                                      "InternalObjectRegistry");

        return engine->singletonInstance<QObjectRegistrySingleton *>(typeId);
    }
    return nullptr;
}

QT_END_NAMESPACE
