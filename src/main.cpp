/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2021 PHYTEC Messtechnik GmbH
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCommandLineParser>
#include <QQmlContext>
#include <QSettings>
#include <QFile>
#include "device_info.hpp"
#include "rauc.hpp"
#include "camera_demo.hpp"

void writeDefaultSettings()
{
    QSettings settings;
    QList<QString> enabledPages = {
        "Camera Demo",
        "Image Viewer",
        "Multimedia",
        "RAUC – Update Client",
        "Multitouch",
        "Device Information",
        "Widget Factory",
        "About PHYTEC"
    };

    settings.beginWriteArray("enabled-pages", enabledPages.size());
    for (int i = 0; i < enabledPages.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", enabledPages.at(i));
    }
    settings.endArray();
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("PHYTEC Messtechnik GmbH");
    app.setApplicationName("qtphy");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({{{"c", "rauc-hawkbit-config"},
                        "RAUC hawkBit client configuration path",
                        "path",
                        "/etc/rauc-hawkbit-updater/config.conf"}});
    parser.process(app);

    QSettings settings;
    QFile file(settings.fileName());
    if (!file.exists()) {
        qWarning() << "Settings file " << file.fileName() << "not found!";
        qWarning() << "Writing new settings file with default values.";
        writeDefaultSettings();
    }

    QVariantList enabledPages;
    int size = settings.beginReadArray("enabled-pages");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        enabledPages.append(settings.value("name").toString());
    }

    qmlRegisterUncreatableMetaObject(
        EnumNamespace::staticMetaObject,
        "Phytec.CameraDemo.Enums",
        1, 0,
        "EnumNamespace",
        "Error: only enums"
    );

    qmlRegisterSingletonType<DeviceInfo>("Phytec.DeviceInfo", 1, 0, "DeviceInfo",
                                         DeviceInfo::singletontypeProvider);
    qmlRegisterType<Rauc>("Phytec.Rauc", 1, 0, "Rauc");
    qmlRegisterType<CameraDemo>("Phytec.CameraDemo", 1, 0, "CameraDemo");

    QQmlApplicationEngine engine;

    CameraImageProvider* cameraFrameProvider = new CameraImageProvider;
    engine.rootContext()->setContextProperty("cameraFrameProvider", cameraFrameProvider);
    engine.addImageProvider(QLatin1String("myCam"), cameraFrameProvider);

    engine.addImportPath("qrc:///themes");
    engine.rootContext()->setContextProperty("raucHawkbitConfigPath",
                                             parser.value("rauc-hawkbit-config"));
    engine.rootContext()->setContextProperty("enabledPages", enabledPages);
    engine.load(QUrl(QStringLiteral("qrc:///main.qml")));

    return app.exec();
}
