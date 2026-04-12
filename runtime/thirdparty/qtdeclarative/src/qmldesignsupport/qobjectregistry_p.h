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

#ifndef QOBJECTREGISTRY_P_H
#define QOBJECTREGISTRY_P_H

#include <QtQmlDesignSupport/qtqmldesignsupportexports.h>

#include <QtQml/qqml.h>
#include <QtQml/qqmlparserstatus.h>

QT_BEGIN_NAMESPACE

class QAbstractObjectRegistryRefPrivate;
class QObjectRegistrySingleton;

class Q_QMLDESIGNSUPPORT_EXPORT QObjectRegistryAttachedType
    : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged FINAL)

    QML_ANONYMOUS

public:
    QObjectRegistryAttachedType(QObject *parent = nullptr);
    ~QObjectRegistryAttachedType();

    QString key() const;
    void setKey(const QString &key);

    void classBegin() override;
    void componentComplete() override;

Q_SIGNALS:
    void keyChanged();

private:
    QString m_key;
    bool m_qmlSetupInProgress = false;
    QObjectRegistrySingleton *m_registry = nullptr;
};

class Q_QMLDESIGNSUPPORT_EXPORT QObjectRegistry : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged REQUIRED FINAL)
    Q_PROPERTY(QObject *target READ target WRITE setTarget NOTIFY targetChanged REQUIRED FINAL)

    QML_ATTACHED(QObjectRegistryAttachedType)

    QML_NAMED_ELEMENT(ObjectRegistry)

public:
    QObjectRegistry(QObject *parent = nullptr);
    ~QObjectRegistry();

    static QObjectRegistryAttachedType *qmlAttachedProperties(QObject *object);

    QString key() const;
    void setKey(const QString &key);

    QObject *target() const;
    void setTarget(QObject *target);

Q_SIGNALS:
    void keyChanged();
    void targetChanged();

private:
    QString m_key;
    QObject *m_target = nullptr;
    QObjectRegistrySingleton *m_registry = nullptr;
};

QT_END_NAMESPACE

#endif // QOBJECTREGISTRY_P_H
