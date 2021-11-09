/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2021 PHYTEC Messtechnik GmbH
 */

pragma Singleton

import QtQuick 2.0

QtObject {
    readonly property color white: "#ffffff"
    readonly property color gray1: "#f5f5f5"
    readonly property color gray2: "#d0d2d3"
    readonly property color gray3: "#95989a"
    readonly property color gray4: "#707070"
    readonly property color black: "#000000"
    readonly property color teal1: "#03c9d6"
    readonly property color teal2: "#02a6b1"
    readonly property color teal3: "#16969e"
    readonly property color red: "#dc3545"
    readonly property color orange: "#fd7e14"
    readonly property color yellow: "#ffc107"
    readonly property color green: "#28a745"
    readonly property color teal: "#20c997"
    readonly property color cyan: "#17a2b8"
    readonly property color blue: "#007bff"
    readonly property color indigo: "#6610f2"
    readonly property color purple: "#6f42c1"
    readonly property color pink: "#e83e8c"

    readonly property int marginSmall: 6
    readonly property int marginRegular: 12
    readonly property int marginBig: 24

    property font font
    font.family: "Roboto"
    font.pointSize: 20

    property QtObject iconFont: QtObject {
        readonly property string arrowLeft: "\uf03b"
        readonly property string dotsThreeVertical: "\uf14a"
        readonly property string code: "\uf10a"
        readonly property string cpu: "\uf119"
        readonly property string file: "\uf167"
        readonly property string folder: "\uf186"
        readonly property string folderOpen: "\uf18c"
        readonly property string frameCorners: "\uf194"
        readonly property string image: "\uf1dd"
        readonly property string lightbulb: "\uf1ed"
        readonly property string list: "\uf1fa"
        readonly property string magnifyingGlassMinus: "\uf20b"
        readonly property string magnifyingGlassPlus: "\uf20c"
        readonly property string numberSquareOne: "\uf245"
        readonly property string play: "\uf27c"
        readonly property string pause: "\uf25e"
        readonly property string stop: "\uf2f7"
        readonly property string skipBack: "\uf2ca"
        readonly property string skipForward: "\uf2cc"
    }
}
