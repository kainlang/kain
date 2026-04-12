// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

//! [0]
// Register object in a QML file
MouseArea {
    ObjectRegistry.key: "MyMouseArea"
}
//! [0]

//! [1]
// In another QML file, refer to the registered object
ObjectRegistryRef {
    id: mouseAreaRef
    key: "MyMouseArea"

    Connections {
        target: mouseAreaRef.object
        function onClicked(mouse) {
            console.log("Mouse clicked")
        }
    }
}
//! [1]

//! [2]
// In C++ code, refer to the registered object, using the QML engine you loaded the QML with
auto ref = new QObjectRegistryRef(engine, "MyMouseArea");
const auto object = ref->object();
connect(object, SIGNAL(clicked(QQuickMouseEvent*)),
        this, SLOT(myClickHandler()));
//! [2]

//! [3]
// In a QML file, register something that gets created multiple times (e.g. a repeater delegate)
Repeater {
    model: 4
    Rectangle {
        id: qmlRepeaterDelegate
        required property int index
        x: 50 * index
        width: 40
        height: 40
        MouseArea {
            property int repeaterIndex: qmlRepeaterDelegate.index
            anchors.fill: parent
            ObjectRegistry.key: "RepeatedMouseArea"
        }
    }
}
//! [3]

//! [4]
// In another QML file, refer to the registered objects
MultiObjectRegistryRef {
    id: repeaterRef
    key: "RepeatedMouseArea"

    onObjectAdded: (object)=> {
        // Connect each object's clicked signal as they are added
        object.clicked.connect(function (mouse) { clickedHandler(object); })
    }

    function clickedHandler(object) {
        console.log("Index clicked: " + object.repeaterIndex)
    }
}
//! [4]

//! [5]
// In C++ code, refer to the registered objects, using the QML engine you loaded the QML with
auto ref = new QMultiObjectRegistryRef(engine, "RepeatedMouseArea");
const auto repeaterObjects = ref->objects();
for (auto obj : repeaterObjects) {
    connect(obj, SIGNAL(clicked(QQuickMouseEvent*)),
            this, SLOT(handleRepeaterObjectClicked()));
}

void MyClass::handleRepeaterObjectClicked()
{
    qDebug() << "Clicked rectangle index: " << sender()->property("repeaterIndex").toInt()
}
//! [5]

//! [6]
// Registering with attached property
Item {
    id: itemToRegister
    ObjectRegistry.key: "MyItem"
}

// Registering with separate ObjectRegistry object
Item {
    id: itemToRegister
    ObjectRegistry {
        target: itemToRegister
        key: "MyItem"
    }
}
//! [6]
