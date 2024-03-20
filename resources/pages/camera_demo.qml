/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2021 PHYTEC Messtechnik GmbH
 */

import QtQuick 2.0
import QtQuick.Dialogs
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Phytec.CameraDemo 1.0
import PhyTheme 1.0
import QtMultimedia
import "../controls"
import "../PhyStyle/demo"
import Phytec.CameraDemo.Enums 1.0

Page {
    header: PhyToolBar {
        title: "Camera Demo"
        buttonBack.onClicked: stack.pop()
        buttonMenu.visible: false
    }
    CameraDemo {
        id: cameraDemo
    }
    // Connect cameraDemo and cameraFrameProvider
    Connections {
        target: cameraDemo
        function onNewImage(image) {
            cameraFrameProvider.updateImage(image)
        }
    }
    // Component.onCompleted: {
    //     cameraDemo.openCamera()
    // }

    RowLayout {
        id: content
        spacing: PhyTheme.marginBig
        anchors.fill: parent

        ColumnLayout {
            Layout.topMargin: PhyTheme.marginBig
            Layout.leftMargin: PhyTheme.marginBig
            Layout.bottomMargin: PhyTheme.marginBig
            Layout.fillWidth: true
            Layout.fillHeight: true

            MessageDialog {
                id: errorDialog1
                visible: (cameraDemo.status == EnumNamespace.WRONG_OVERLAYS) ? true : false

                text: "Unconfigured camera found!"
                informativeText: "Seems like a camera is connected but the wrong devicetree overlays have been selected.\n" +
                "The detectCamera script recommends the following devicetree overlays:\n\n" + cameraDemo.recommendedOverlays + "\n\n" +
                "Do you want to load these overlays and reboot?"
                buttons: MessageDialog.Ok | MessageDialog.Cancel
                onAccepted: cameraDemo.reloadOverlays()
            }
            MessageDialog {
                id: errorDialog2
                visible: (cameraDemo.status == EnumNamespace.NO_CAM) ? true : false

                text: "No Camera Found!"
                informativeText: "No camera found on the CSI interfaces of the board!"
                buttons: MessageDialog.Ok
            }
            MessageDialog {
                id: errorDialog3
                visible: (cameraDemo.status == EnumNamespace.ISP_UNAVAILABLE) ? true : false

                text: "ISP overlay not loaded!"
                informativeText: "Your hardware has an ISP but you did not load the ISP overlay for all connected cameras.\n" +
                "The detectCamera script recommends the following devicetree overlays:\n\n" + cameraDemo.recommendedOverlays + "\n\n" +
                "Do you want to load these overlays and reboot?"
                buttons: MessageDialog.Ok | MessageDialog.Cancel
                onAccepted: cameraDemo.reloadOverlays()
            }
            MessageDialog {
                id: errorDialog4
                visible: (cameraDemo.status == EnumNamespace.ISP_UNSUPPORTED) ? true : false

                text: "ISP is not supported by your image!"
                informativeText: "Your hardware has an ISP but the image you are using does not support it. \n" +
                "Make sure you are using the phytec-vison-image to use ISP features."
                buttons: MessageDialog.Ok
            }
            MessageDialog {
                id: errorDialog5
                visible: (cameraDemo.status == EnumNamespace.ISI_UNAVAILABLE) ? true : false

                text: "ISI overlay not loaded!"
                informativeText: "Your hardware has an ISI but you did not load the ISI overlay for all connected cameras.\n" +
                "The detectCamera script recommends the following devicetree overlays:\n\n" + cameraDemo.recommendedOverlays + "\n\n" +
                "Do you want to load these overlays and reboot?"
                buttons: MessageDialog.Ok | MessageDialog.Cancel
                onAccepted: cameraDemo.reloadOverlays()
            }
            Image {
                id: streamImage
                Layout.fillHeight: true
                Layout.fillWidth: true
                fillMode: Image.PreserveAspectFit

                property bool counter: false
                source: "image://myCam/image"
                asynchronous: false
                cache: false

                function reloadImage() {
                    counter = !counter
                    source = "image://myCam/image?id=" + counter
                }
            }
            Connections {
                target: cameraFrameProvider

                function onImageChanged() {
                    streamImage.reloadImage()
                }
            }
        }
        ColumnLayout {
            Layout.topMargin: PhyTheme.marginBig
            Layout.rightMargin: PhyTheme.marginBig
            Layout.bottomMargin: PhyTheme.marginBig
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Open Camera Button
            Button {
                id: startButton
                Layout.fillHeight: true
                text: "Open Camera"
                font.pointSize: 14
                onClicked: {
                    cameraDemo.openCamera()
                }
                Layout.alignment: Qt.AlignVCenter
            }
            RowLayout {
                Layout.fillHeight: true

                Label {
                    visible: (cameraDemo.hostHardware.hasISP && cameraDemo.hostHardware.hasISI)
                    enabled: (cameraDemo.ispAvailable)
                    text: "ISP"
                    Layout.alignment: Qt.AlignVCenter
                }
                Switch {
                    id: videoSourceSwitch
                    visible: (cameraDemo.hostHardware.hasISP && cameraDemo.hostHardware.hasISI)
                    enabled:(cameraDemo.isiAvailable && cameraDemo.ispAvailable)
                    checked: cameraDemo.videoSrc
                    onCheckedChanged: {
                        cameraDemo.setVideoSource(checked)
                    }
                    Layout.alignment: Qt.AlignVCenter
                }
                Label {
                    visible: (cameraDemo.hostHardware.hasISP && cameraDemo.hostHardware.hasISI)
                    enabled: (cameraDemo.isiAvailable)
                    text: "ISI"
                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: 10
                }

                Label {
                    visible: (cameraDemo.hostHardware.hasDualCam)
                    text: "CSI1"
                    Layout.alignment: Qt.AlignVCenter
                }
                Switch {
                    id: interfaceSwitch
                    visible: (cameraDemo.hostHardware.hasDualCam)
                    enabled: (cameraDemo.hostHardware.dualCamAvailable)
                    checked: (cameraDemo.interface == 2) ? 1 : 0

                    onCheckedChanged: {
                        cameraDemo.setInterface(checked)
                    }
                    Layout.alignment: Qt.AlignVCenter
                }
                Label {
                    visible: (cameraDemo.hostHardware.hasDualCam)
                    text: "CSI2"
                    Layout.alignment: Qt.AlignVCenter
                }
            }
            // Camera and Sensor Name
            Row {
                Layout.fillHeight: true

                Label {
                    text: "Connected Camera: "
                }
                Label {
                    id: cameraNameLabel
                    text: cameraDemo.cameraName
                }
            }
            // Interface
            Row {
                Layout.fillHeight: true

                Label {
                    text: "Interface: "
                }
                Label {
                    id: interfaceLabel
                    text: cameraDemo.interfaceString
                }
            }
            // Resolution
            Row {
                Layout.fillHeight: true

                Label {
                    text: "Resolution: "
                }
                Label {
                    id: resolutionLabel
                    text: cameraDemo.framesize
                }
            }

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                currentIndex: !cameraDemo.videoSrc
                TabButton {
                    Layout.fillWidth: true
                    id: sensorControlsTabButton
                    font.pointSize: 14
                    background: Rectangle {
                        color: "transparent"
                        implicitWidth: 100
                        implicitHeight: 50
                        opacity: enabled ? 1 : 0.3
                        Rectangle {
                            width: parent.width
                            height: 2 // Height of the bottom margin
                            color: tabBar.currentIndex === 0 ? "#2196F3" : "transparent" // Color of the bottom margin
                            anchors.bottom: parent.bottom // Align at the bottom of the TabButton
                        }
                    }
                    contentItem: Text {
                        text: "Sensor controls"
                        font: sensorControlsTabButton.font
                        opacity: enabled ? 1.0 : 0.3
                        color: tabBar.currentIndex == 0 ? "#2196F3" : PhyTheme.gray4
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                TabButton {
                    Layout.fillWidth: true
                    id: ispControlsTabButton
                    visible: cameraDemo.hostHardware.hasISP
                    font.pointSize: 14
                    background: Rectangle {
                        color: "transparent"
                        implicitWidth: 100
                        implicitHeight: 50
                        opacity: enabled ? 1 : 0.3
                        Rectangle {
                            width: parent.width
                            height: 2 // Height of the bottom margin
                            color: tabBar.currentIndex === 1 ? "#2196F3" : "transparent" // Color of the bottom margin
                            anchors.bottom: parent.bottom // Align at the bottom of the TabButton
                        }
                    }
                    contentItem: Text {
                        text: "ISP controls"
                        font: ispControlsTabButton.font
                        opacity: enabled ? 1.0 : 0.3
                        color: tabBar.currentIndex == 1 ? "#2196F3" : PhyTheme.gray4
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }


            StackLayout {
                // width: parent.width
                currentIndex: tabBar.currentIndex
                Layout.fillHeight: true

                // Sensor Controls
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    id: isiTab
                    // Horizontal Flip
                    CheckBox {
                        id: flipHorizontalCheckbox
                        Layout.fillHeight: true
                        font.pointSize: 14
                        text: "Flip Horizontal"
                        enabled: cameraDemo.flipSupported
                        checked: cameraDemo.flipHorizontal
                        onClicked: {
                            cameraDemo.setFlipHorizontal(flipHorizontalCheckbox.checked)
                        }
                    }
                    // Vertical Flip
                    CheckBox {
                        id: flipVerticalCheckbox
                        Layout.fillHeight: true
                        font.pointSize: 14
                        text: "Flip Vertical"
                        enabled: cameraDemo.flipSupported
                        checked: cameraDemo.flipVertical
                        onClicked: {
                            cameraDemo.setFlipVertical(flipVerticalCheckbox.checked)
                        }
                    }
                    // Auto Exposure
                    CheckBox {
                        id: autoExposureCheckbox
                        Layout.fillHeight: true
                        font.pointSize: 14
                        text: "Auto Exposure & Gain"
                        enabled: (cameraDemo.videoSrc && cameraDemo.hasAutoExposure) // Enable only on ISI
                        checked: (cameraDemo.videoSrc && cameraDemo.autoExposure);
                        onClicked: {
                            cameraDemo.setAutoExposure(autoExposureCheckbox.checked)
                        }
                    }
                    // Exposure Slider
                    Label {
                        Layout.fillHeight: true
                        text: "Exposure"
                        opacity: exposureSlider.enabled ? 1 : 0.3
                    }
                    Slider {
                        id: exposureSlider
                        Layout.fillHeight: true
                        // disable only on ISI and whe no Auto Exposure is enabled
                        enabled: !cameraDemo.autoExposure && cameraDemo.videoSrc
                        from: 0
                        value: cameraDemo.autoExposure ? 0 : cameraDemo.exposure
                        to: 30000 // limit exposure time to 30ms
                        onMoved: {
                            cameraDemo.setExposure(exposureSlider.value)
                        }
                    }
                    // Analog Gain Slider
                    Label {
                        Layout.fillHeight: true
                        text: "Analog Gain"
                        opacity: analogGainSlider.enabled ? 1 : 0.3
                    }
                    Slider {
                        id: analogGainSlider
                        Layout.fillHeight: true
                        // disable when isp or sensor Auto Exposure is enabled
                        enabled: !cameraDemo.autoExposure && cameraDemo.videoSrc
                        from: 1000
                        value: cameraDemo.autoExposure ? 0 : cameraDemo.analogGain
                        to: 14000
                        onMoved: {
                            cameraDemo.setAnalogGain(analogGainSlider.value)
                        }
                    }
                    // Analog Gain Slider
                    Label {
                        Layout.fillHeight: true
                        text: "Digital Gain"
                        opacity: digitalGainSlider.enabled ? 1 : 0.3
                    }
                    Slider {
                        id: digitalGainSlider
                        Layout.fillHeight: true
                        // disable when isp or sensor Auto Exposure is enabled
                        enabled: !cameraDemo.autoExposure && cameraDemo.videoSrc
                        from: 1000
                        value:  cameraDemo.autoExposure ? 0 : cameraDemo.digitalGain
                        to: 14000
                        onMoved: {
                            cameraDemo.setDigitalGain(digitalGainSlider.value)
                        }
                    }
                }

                // ISP Controls
                ColumnLayout {
                    id: ispTab
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Text {
                        text: "! ISP overlay not loaded !"
                        visible: !cameraDemo.ispAvailable
                        Layout.alignment: Qt.AlignHCenter
                        font.pointSize: 14
                        color: "red"
                    }
                    Text {
                        text: "! This board has no ISP !"
                        Layout.alignment: Qt.AlignHCenter
                        visible: !cameraDemo.hostHardware.hasISP
                        font.pointSize: 14
                        color: "red"
                    }
                    Text {
                        text: "! Your Image has no ISP support !"
                        Layout.alignment: Qt.AlignHCenter
                        visible: (cameraDemo.status == EnumNamespace.ISP_UNSUPPORTED) ? true : false
                        font.pointSize: 14
                        color: "red"
                    }

                    // Auto Exposure (ISP)
                    CheckBox {
                        id: aecCheckbox
                        // visible: cameraDemo.hostHardware.hasISP
                        enabled: (!cameraDemo.videoSrc && cameraDemo.ispAvailable)
                        Layout.fillHeight: true
                        font.pointSize: 14
                        text: "Auto Exposure & Gain"
                        checked: !cameraDemo.videoSrc
                        onClicked: {
                            cameraDemo.setAec(aecCheckbox.checked)
                        }
                    }

                    // Auto White Balance
                    CheckBox {
                        id: awbCheckbox
                        // visible: cameraDemo.hostHardware.hasISP
                        enabled: (!cameraDemo.videoSrc && cameraDemo.ispAvailable)
                        Layout.fillHeight: true
                        font.pointSize: 14
                        text: "Auto White Balance"
                        checked: !cameraDemo.videoSrc
                        onClicked: {
                            cameraDemo.setAwb(awbCheckbox.checked)
                        }
                    }
                    // Lens Shading Correction
                    CheckBox {
                        id: lscCheckbox
                        // visible: cameraDemo.hostHardware.hasISP
                        enabled: !cameraDemo.videoSrc && cameraDemo.ispAvailable
                        Layout.fillHeight: true
                        font.pointSize: 14
                        text: "Lens Shading Correction"
                        checked: !cameraDemo.videoSrc
                        onClicked: {
                            cameraDemo.setLsc(lscCheckbox.checked)
                        }
                    }
                    // Exposure Slider
                    Label {
                        Layout.fillHeight: true
                        text: "Exposure"
                        opacity: ispExposureSlider.enabled ? 1 : 0.3
                    }
                    Slider {
                        id: ispExposureSlider
                        Layout.fillHeight: true
                        // disable when isp or sensor Auto Exposure is enabled
                        enabled: !cameraDemo.videoSrc && !aecCheckbox.checked
                        from: 0
                        value: cameraDemo.ispExposure
                        // cameraDemo.autoExposure ? 0 : cameraDemo.exposure
                        to: 100 // limit exposure time to 30ms
                        onMoved: {
                            cameraDemo.setISPExposure(ispExposureSlider.value)
                        }
                    }
                    // Gain Slider
                    Label {
                        Layout.fillHeight: true
                        text: "Gain"
                        opacity: ispGainSlider.enabled ? 1 : 0.3
                    }
                    Slider {
                        id: ispGainSlider
                        Layout.fillHeight: true
                        // disable when isp or sensor Auto Exposure is enabled
                        enabled: !cameraDemo.videoSrc && !aecCheckbox.checked
                        from: 1
                        value: cameraDemo.ispGain
                        to: 100
                        onMoved: {
                            cameraDemo.setISPGain(ispGainSlider.value)
                        }
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
