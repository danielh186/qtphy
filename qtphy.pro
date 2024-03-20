TEMPLATE = app
TARGET = qtphy

QT += qml quick dbus widgets multimedia

LIBS += -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstapp-1.0

SOURCES += \
    src/main.cpp \
    src/device_info.cpp \
    src/rauc.cpp \
    src/camera_demo.cpp

HEADERS += \
    src/device_info.hpp \
    src/rauc.hpp \
    src/camera_demo.hpp

RESOURCES += \
    resources/resources.qrc

target.path = $$(bindir)
INSTALLS += target
