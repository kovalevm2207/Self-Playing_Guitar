import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 840
    minimumWidth: 960
    minimumHeight: 700
    visible: true
    title: "SelfGuitar · Моя музыка"
    color: "#101214"
    font.family: "Segoe UI"
    property color ink: "#f2f0eb"
    property color muted: "#8e9498"
    property color accent: "#c4ed91"
    property int page: 0
    property var current: catalog.selectedArrangementId.length ? catalog.selectedArrangement : null
    property bool currentSupported: current !== null && current.strings.every(function (number) {
        return (transfer.supportedStringMask & (1 << (number - 1))) !== 0;
    })
    onPageChanged: catalog.favoritesOnly = page === 1
    Connections {
        target: catalog
        function onSelectionChanged() {
            tempo.value = catalog.selectedArrangementId.length ? catalog.selectedArrangement.bpm : 100;
        }
    }
    function duration(ms) {
        var s = Math.round(ms / 1000);
        return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + s % 60;
    }

    component Txt: Label {
        color: root.ink
        font.pixelSize: 14
    }
    component Action: Button {
        id: action
        property bool primary: false
        implicitHeight: 44
        leftPadding: 18
        rightPadding: 18
        contentItem: Text {
            text: action.text
            color: action.enabled ? (action.primary ? "#182214" : root.ink) : "#666c6a"
            font.pixelSize: 13
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 10
            color: !action.enabled ? "#272c29" : action.primary ? (action.down ? "#a5cc77" : root.accent) : action.hovered ? "#34393b" : "#252a2d"
            border.color: action.primary ? "transparent" : "#373c3f"
        }
    }
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: 205
            Layout.fillHeight: true
            color: "#171a1c"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 12
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 34
                        height: 38
                        radius: 10
                        color: root.accent
                        Text {
                            anchors.centerIn: parent
                            text: "♫"
                            font.pixelSize: 25
                            color: "#172214"
                        }
                    }
                    Txt {
                        text: "selfguitar"
                        font.pixelSize: 21
                        font.weight: Font.Bold
                    }
                }
                Txt {
                    text: "МУЗЫКА В ДВИЖЕНИИ"
                    color: root.muted
                    font.pixelSize: 9
                    font.letterSpacing: 1.4
                    Layout.topMargin: 5
                    Layout.bottomMargin: 36
                }
                Repeater {
                    model: ["♫   Каталог песен", "♡   Избранное"]
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        height: 44
                        radius: 10
                        color: root.page === index ? "#2a3429" : "transparent"
                        Txt {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: root.page === index ? root.accent : root.muted
                            font.pixelSize: 13
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.page = index
                        }
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 128
                    radius: 12
                    color: "#202527"
                    border.color: "#303638"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 7
                        Txt {
                            text: "ВАША ГИТАРА"
                            color: root.muted
                            font.pixelSize: 9
                            font.letterSpacing: 1.1
                        }
                        Txt {
                            text: "Arduino Uno"
                            font.weight: Font.DemiBold
                        }
                        Txt {
                            text: transfer.connected ? "●  Подключена по USB" : "○  Не подключена"
                            color: transfer.connected ? root.accent : root.muted
                            font.pixelSize: 11
                        }
                        Action {
                            text: "Подключение  ↗"
                            Layout.fillWidth: true
                            implicitHeight: 32
                            onClicked: {
                                transfer.refreshPorts();
                                connection.open();
                            }
                        }
                    }
                }
                Txt {
                    text: "DESKTOP / v0.1"
                    color: "#636b6f"
                    font.pixelSize: 9
                    Layout.topMargin: 10
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 30
            spacing: 20
            RowLayout {
                Layout.fillWidth: true
                Txt {
                    text: "БИБЛИОТЕКА  /  " + (root.page === 1 ? "ИЗБРАННОЕ" : "КАТАЛОГ")
                    color: root.muted
                    font.pixelSize: 10
                    font.letterSpacing: 1.8
                }
                Item {
                    Layout.fillWidth: true
                }
                Txt {
                    text: "●  " + (transfer.connected ? "UNO ONLINE" : "USB OFFLINE")
                    color: transfer.connected ? root.accent : root.muted
                    font.pixelSize: 10
                    font.letterSpacing: 1
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 150
                radius: 18
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: "#28352d"
                    }
                    GradientStop {
                        position: 1
                        color: "#1c2529"
                    }
                }
                clip: true
                Repeater {
                    model: 6
                    Rectangle {
                        required property int index
                        x: parent.width * 0.66 + index * 24
                        y: -40
                        width: 1 + index * 0.35
                        height: 260
                        rotation: 26
                        color: "#68816a"
                        opacity: 0.35
                    }
                }
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.leftMargin: 26
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Txt {
                        text: "ВЫБЕРИТЕ. ЗАГРУЗИТЕ. СОЗДАВАЙТЕ."
                        font.pixelSize: 9
                        font.letterSpacing: 1.8
                        color: root.accent
                    }
                    Txt {
                        text: root.page === 1 ? "Любимые мелодии" : "Музыка начинается здесь"
                        font.pixelSize: 29
                        font.weight: Font.DemiBold
                    }
                    Txt {
                        text: "Аранжировки для вашей гитары — от одной до шести струн."
                        font.pixelSize: 12
                        color: "#b3bfb5"
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                TextField {
                    objectName: "catalogSearch"
                    Layout.fillWidth: true
                    implicitHeight: 44
                    color: root.ink
                    placeholderTextColor: root.muted
                    placeholderText: "Поиск песни или автора…"
                    font.pixelSize: 13
                    leftPadding: 16
                    background: Rectangle {
                        color: "#1c2023"
                        radius: 10
                        border.color: parent.activeFocus ? root.accent : "#303639"
                    }
                    onTextChanged: catalog.searchText = text
                }
                ComboBox {
                    id: filters
                    objectName: "stringFilter"
                    model: ["Все аранжировки", "До 1 струны", "До 2 струн", "До 3 струн", "До 4 струн", "До 5 струн", "До 6 струн"]
                    implicitWidth: 180
                    implicitHeight: 44
                    onCurrentIndexChanged: catalog.maxStrings = currentIndex
                    palette.button: "#252a2d"
                    palette.buttonText: root.ink
                    palette.base: "#252a2d"
                    palette.text: root.ink
                    palette.highlight: "#3c5034"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 22
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    RowLayout {
                        Txt {
                            text: root.page === 1 ? "Избранные аранжировки" : "Каталог аранжировок"
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Txt {
                            text: catalog.arrangements.length + " в списке"
                            color: root.muted
                            font.pixelSize: 11
                        }
                    }
                    ListView {
                        id: songList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 10
                        clip: true
                        model: catalog.arrangements
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            id: card
                            required property var modelData
                            width: songList.width
                            height: 104
                            radius: 13
                            color: catalog.selectedArrangementId === modelData.id ? "#252e27" : "#1c2023"
                            border.color: catalog.selectedArrangementId === modelData.id ? "#79975b" : "#2b3134"
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: catalog.selectArrangement(card.modelData.id)
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 14
                                Rectangle {
                                    width: 52
                                    height: 64
                                    radius: 9
                                    color: card.modelData.color
                                    Text {
                                        anchors.centerIn: parent
                                        text: card.modelData.mark
                                        font.pixelSize: 26
                                        color: "#e9eddf"
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Txt {
                                        text: card.modelData.title
                                        font.weight: Font.DemiBold
                                        font.pixelSize: 15
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Txt {
                                        text: card.modelData.author + " · " + card.modelData.arrangement
                                        color: root.muted
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Txt {
                                        text: card.modelData.stringCount + " стр.  ·  " + card.modelData.bpm + " BPM  ·  " + root.duration(card.modelData.durationMs)
                                        color: root.accent
                                        font.pixelSize: 11
                                    }
                                }
                                Button {
                                    text: card.modelData.favorite ? "♥" : "♡"
                                    implicitWidth: 30
                                    implicitHeight: 36
                                    background: Item {}
                                    contentItem: Text {
                                        text: parent.text
                                        color: card.modelData.favorite ? root.accent : root.muted
                                        font.pixelSize: 23
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    onClicked: catalog.toggleFavorite(card.modelData.id)
                                }
                            }
                        }
                        Txt {
                            anchors.centerIn: parent
                            visible: songList.count === 0
                            text: "Ничего не найдено\nПопробуйте другой поиск или фильтр"
                            color: root.muted
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 302
                    Layout.fillHeight: true
                    color: "#1b1f21"
                    radius: 16
                    border.color: "#2d3436"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: availableWidth
                            clip: true
                            ColumnLayout {
                                width: parent.width
                                spacing: 15
                                Txt {
                                    text: "ВЫБРАННАЯ АРАНЖИРОВКА"
                                    color: root.muted
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.3
                                }
                                Txt {
                                    text: root.current ? root.current.title : "Каталог пуст"
                                    font.pixelSize: 24
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                Txt {
                                    text: root.current ? root.current.description : ""
                                    color: root.muted
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#343a3c"
                                }
                                RowLayout {
                                    Txt {
                                        text: "Нужно струн"
                                        color: root.muted
                                        Layout.fillWidth: true
                                    }
                                    Txt {
                                        text: root.current ? root.current.stringCount + " из 6" : "—"
                                        color: root.accent
                                        font.weight: Font.Bold
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    Repeater {
                                        model: 6
                                        ColumnLayout {
                                            required property int index
                                            property bool used: root.current && root.current.strings.indexOf(index + 1) >= 0
                                            Layout.fillWidth: true
                                            spacing: 6
                                            Rectangle {
                                                Layout.fillWidth: true
                                                height: 37
                                                radius: 7
                                                color: parent.used ? "#34452e" : "#252a2c"
                                                border.color: parent.used ? "#6e8b55" : "#383e40"
                                                Txt {
                                                    anchors.centerIn: parent
                                                    text: index + 1
                                                    color: parent.parent.used ? root.accent : "#657075"
                                                }
                                            }
                                            Txt {
                                                text: ["E₄", "B₃", "G₃", "D₃", "A₂", "E₂"][index]
                                                color: root.muted
                                                font.pixelSize: 10
                                                Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }
                                }
                                Txt {
                                    text: "1 — самая тонкая струна. Стандартный строй."
                                    color: root.muted
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Txt {
                                        text: "Темп"
                                        color: root.muted
                                        Layout.fillWidth: true
                                    }
                                    Txt {
                                        text: Math.round(tempo.value) + " BPM"
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Slider {
                                    id: tempo
                                    objectName: "playbackTempo"
                                    Layout.fillWidth: true
                                    from: 40
                                    to: 220
                                    stepSize: 1
                                    value: 100
                                    Component.onCompleted: value = root.current ? root.current.bpm : 100
                                    enabled: !transfer.busy
                                }
                                Txt {
                                    text: root.current ? root.current.noteCount + " нот · " + root.duration(root.current.durationMs * root.current.bpm / tempo.value) + " при этом темпе" : ""
                                    color: root.muted
                                    font.pixelSize: 11
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#343a3c"
                        }
                        Txt {
                            text: "Загрузка в Arduino Uno"
                            font.weight: Font.DemiBold
                        }
                        Txt {
                            text: catalog.error.length ? catalog.error : transfer.status
                            color: transfer.connected ? root.accent : root.muted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            font.pixelSize: 11
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            value: transfer.progress
                            visible: transfer.busy || transfer.progress > 0
                        }
                        Txt {
                            text: root.currentSupported ? "Стенд: струна 1 · самая тонкая E4" : "Эта аранжировка требует другие струны. Выберите версию для струны 1 (E4)."
                            visible: transfer.connected && transfer.supportedStringMask === 1
                            color: root.currentSupported ? root.muted : "#edc78f"
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: !transfer.playing
                            spacing: 8
                            Action {
                                objectName: "uploadScore"
                                text: transfer.busy ? "Передаём…" : "↓ Загрузить"
                                primary: !transfer.canPlay
                                Layout.fillWidth: true
                                enabled: transfer.connected && !transfer.busy && root.currentSupported
                                onClicked: transfer.upload(catalog.selectedArrangementId, Math.round(tempo.value))
                            }
                            Action {
                                objectName: "playScore"
                                text: "▶ Исполнить"
                                primary: transfer.canPlay
                                Layout.fillWidth: true
                                enabled: transfer.canPlay
                                onClicked: transfer.play()
                            }
                        }
                        Action {
                            objectName: "stopPlayback"
                            text: "■  Остановить"
                            visible: transfer.playing
                            Layout.fillWidth: true
                            onClicked: transfer.stop()
                        }
                        Action {
                            text: "Отменить"
                            visible: transfer.busy && !transfer.playing
                            Layout.fillWidth: true
                            onClicked: transfer.cancel()
                        }
                        Txt {
                            text: "Исполнение работает в симуляции приводов. Реальное движение потребует калибровки устройства."
                            color: "#879186"
                            wrapMode: Text.WordWrap
                            font.pixelSize: 10
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            Txt {
                text: "Локальный каталог  ·  USB 115200  ·  Песни хранятся в RAM Uno до отключения питания"
                color: "#626d72"
                font.pixelSize: 10
            }
        }
    }
    Dialog {
        id: connection
        anchors.centerIn: parent
        width: 450
        modal: true
        title: "Подключить Arduino Uno"
        palette.window: "#202527"
        palette.windowText: root.ink
        palette.text: root.ink
        palette.buttonText: root.ink
        background: Rectangle {
            radius: 16
            color: "#202527"
            border.color: "#414a42"
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 15
            Txt {
                text: "Подключите Uno USB-кабелем и выберите порт. На плате должна быть прошивка song_receiver."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: root.muted
            }
            RowLayout {
                ComboBox {
                    id: portChoice
                    model: transfer.ports
                    Layout.fillWidth: true
                    implicitHeight: 42
                }
                Action {
                    text: "Обновить"
                    onClicked: transfer.refreshPorts()
                }
            }
            Txt {
                text: catalog.error.length ? catalog.error : transfer.status
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: root.accent
                font.pixelSize: 12
            }
            RowLayout {
                Action {
                    text: "Закрыть"
                    onClicked: connection.close()
                }
                Item {
                    Layout.fillWidth: true
                }
                Action {
                    text: transfer.connected ? "Отключить" : "Подключить"
                    primary: true
                    enabled: !transfer.busy && (transfer.connected || portChoice.count > 0)
                    onClicked: {
                        if (transfer.connected)
                            transfer.disconnectPort();
                        else
                            transfer.connectPort(portChoice.currentText);
                    }
                }
            }
        }
    }
}
