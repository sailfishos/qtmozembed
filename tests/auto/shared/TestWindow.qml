import QtQuick 2.0

Rectangle {
    anchors.fill: parent
    color: "red"
    focus: true

    property bool mozViewInitialized
    property var mozView
    property int createParentID
    property alias name: testCaseName.text

    Text {
        id: testCaseName

        anchors.centerIn: parent
        font.pixelSize: 40
        color: "white"
    }
}
