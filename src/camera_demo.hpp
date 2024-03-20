/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2021 PHYTEC Messtechnik GmbH
 */

#ifndef CAMERA_DEMO_HPP
#define CAMERA_DEMO_HPP

#include <QImage>
#include <QTimer>
#include <QQuickImageProvider>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <json.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

#define V4L2_CID_VIV_EXTCTRL 0x98F901

namespace EnumNamespace
{
    Q_NAMESPACE
    enum Video_srcs
    {
        ISP,
        ISI
    };
    enum Camera_status
    {
        ACTIVE,
        READY,
        UNCONNECTED,
        ERROR
    };
    enum Status
    {
        OK,
        NO_CAM,
        WRONG_OVERLAYS,
        ISP_UNAVAILABLE,
        ISI_UNAVAILABLE,
        ISP_UNSUPPORTED,
    };
    enum CSI_interface
    {
        CSI1,
        CSI2
    };
    Q_ENUM_NS(Video_srcs);
    Q_ENUM_NS(Camera_status);
    Q_ENUM_NS(Status);
    Q_ENUM_NS(CSI_interface);
}
using namespace EnumNamespace;

struct Sensor
{
    std::string camera_name;
    std::string name;
    bool hasAutoExposure;
    bool flip_supported;
    int sensor_width;
    int sensor_height;
    int frame_width;
    int frame_height;
    int offset_x;
    int offset_y;
};

struct Host_hardware
{
    Q_GADGET
public:
    std::string hostname;
    bool hasISP;
    bool hasISI;
    bool hasDualCam;
    bool dualCamAvailable;
    Q_PROPERTY(bool hasISP MEMBER hasISP)
    Q_PROPERTY(bool hasISI MEMBER hasISI)
    Q_PROPERTY(bool hasDualCam MEMBER hasDualCam)
    Q_PROPERTY(bool dualCamAvailable MEMBER dualCamAvailable)
};

extern Sensor SENSORS[];
extern Host_hardware HOST_HARDWARE[];
float mapToRange(float value, float inMin, float inMax, float outMin, float outMax);

class PhyCam
{
public:
    PhyCam(const int _interface, Host_hardware *_host_hardware);
    PhyCam(){};
    ~PhyCam();

    int checkISPAvailable();

    Camera_status status = UNCONNECTED;
    bool isColor = 1;
    int64_t pixel_rate = 1;
    bool ispAvailable = false;
    bool isiAvailable = false;

    std::string device = ""; // /dev/cam-csi1-port0   (CAM)
    int device_fd = -1;      // file descriptor of device (subdevFd)
    int isp_fd = -1;

    Host_hardware *host_hardware;

    int csi_interface = -1;
    int port = -1;

    Sensor *sensor = &SENSORS[0];
    Video_srcs video_src = ISP;
    std::string setup_pipeline_command = "";
    std::string isp_pipeline = "";
    std::string isi_pipeline = "";

    int getSensor();
    int setup_pipeline();
};

class CameraImageProvider : public QQuickImageProvider
{
    Q_OBJECT

public:
    CameraImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

public slots:
    void updateImage(const QImage image);

signals:
    void imageChanged();

private:
    QImage image;
};

static GstFlowReturn on_new_sample_callback(GstAppSink *sink, gpointer user_data);

class CameraDemo : public QObject
{
    Q_OBJECT

    // Camera Information
    Q_PROPERTY(QString cameraName // Camera Name
                   READ getCameraName
                       NOTIFY sensorChanged);
    Q_PROPERTY(QString framesize // Framesize
                   READ getFramesize
                       NOTIFY sensorChanged);
    Q_PROPERTY(bool hasAutoExposure // Auto Exposure availability on sensor
                   READ getHasAutoExposure
                       NOTIFY sensorChanged);

    Q_PROPERTY(Host_hardware hostHardware // Host Hardware
                   READ getHostHardware
                       NOTIFY hostHardwareChanged);

    Q_PROPERTY(int interface // Interface
                   READ getInterface
                       NOTIFY interfaceChanged);
    Q_PROPERTY(QString interfaceString // Interface string
                   READ getInterfaceString
                       NOTIFY interfaceChanged);
    Q_PROPERTY(bool ispAvailable // ISP available on selected camera?
                   READ getIspAvailable
                       NOTIFY interfaceChanged);
    Q_PROPERTY(bool isiAvailable // ISI available on selected camera?
                   READ getIsiAvailable
                       NOTIFY interfaceChanged);
    Q_PROPERTY(int videoSrc // Video Source (ISP or ISI)
                   READ getVideoSrc
                       NOTIFY videoSrcChanged);
    // V4L2 Controls
    Q_PROPERTY(bool autoExposure // Auto Exposure
                   READ getAutoExposure
                       NOTIFY autoExposureChanged);
    Q_PROPERTY(bool flipSupported // Flip Supported
                   READ getFlipSupported
                       NOTIFY interfaceChanged);
    Q_PROPERTY(bool flipVertical // Flip Vertical
                   READ getFlipVertical
                       NOTIFY flipVerticalChanged);
    Q_PROPERTY(bool flipHorizontal // Flip Horizontal
                   READ getFlipHorizontal
                       NOTIFY flipHorizontalChanged);
    Q_PROPERTY(int exposure // Exposure
                   READ getExposure
                       NOTIFY exposureChanged);
    Q_PROPERTY(int analogGain // Analog Gain
                   READ getAnalogGain
                       NOTIFY analogGainChanged);
    Q_PROPERTY(int digitalGain // Digital Gain
                   READ getDigitalGain
                       NOTIFY digitalGainChanged);
    Q_PROPERTY(int ispExposure // ISP Exposure
                   READ getIspExposure
                       NOTIFY exposureChanged);
    Q_PROPERTY(int ispGain // ISP Gain
                   READ getIspGain
                       NOTIFY ispGainChanged);
    // Status and Errors
    Q_PROPERTY(QString recommendedOverlays // Recommended overlays
                   READ getRecommendedOverlays
                       NOTIFY recommendedOverlaysChanged);
    Q_PROPERTY(Status status
                   READ getStatus
                       NOTIFY statusChanged);

public:
    CameraDemo(QObject *parent = nullptr);
    ~CameraDemo();
    void updateFrame();
    Q_INVOKABLE void reloadOverlays();
    GstFlowReturn on_new_sample(GstAppSink *sink);

private:
    QTimer sliderUpdateTimer;
    void startStream(std::string pipeline_string);
    void stopStream();
    void updateSliders();

    // ISP will not be available directly after boot -> check periodically until available (max 30 times)
    QTimer checkIspTimer;
    void delayedStatusUpdate();


    GstElement *pipeline = nullptr;
    GstElement *appsink = nullptr;
    GstBus *bus = nullptr;

    Host_hardware *host_hardware = &HOST_HARDWARE[0];
    PhyCam *cam1 = nullptr;
    PhyCam *cam2 = nullptr;
    PhyCam *CAM = nullptr;

    Status STATUS = OK;
    QString RECOMMENDED_OVERLAYS = "";

    int isp_ioctl(const char *cmd, json &jsonRequest, json &jsonResponse);

signals:
    void newImage(QImage image);

    void sensorChanged();
    void autoExposureChanged();
    void flipVerticalChanged();
    void flipHorizontalChanged();
    void exposureChanged();
    void analogGainChanged();
    void digitalGainChanged();
    void ispGainChanged();
    void videoSrcChanged();
    void interfaceChanged();
    void recommendedOverlaysChanged();
    void statusChanged();
    void csiPortChanged();
    void hostHardwareChanged();

public slots:
    void openCamera();
    void detectCameras();

    QString getCameraName() const;
    QString getFramesize() const;
    QString getRecommendedOverlays() const;
    QString getInterfaceString() const;

    int getInterface() const;
    int getVideoSrc() const;
    bool getIspAvailable() const;
    bool getIsiAvailable() const;

    bool getAutoExposure();
    bool getHasAutoExposure();
    bool getFlipSupported();
    bool getFlipHorizontal();
    bool getFlipVertical();

    int getExposure();
    int getAnalogGain();
    int getDigitalGain();
    int getIspGain();
    int getIspExposure();
    Status getStatus();
    Host_hardware getHostHardware();

    void setAutoExposure(bool value);
    void setFlipVertical(bool value);
    void setFlipHorizontal(bool value);
    void setExposure(int value);
    void setDigitalGain(int value);
    void setAnalogGain(int value);
    void setISPGain(int gain);
    void setISPExposure(int exposure);

    void setVideoSource(Video_srcs value);
    void setInterface(CSI_interface value);

    void setAec(bool value);
    void setAwb(bool value);
    void setLsc(bool value);
};

#endif /* CAMERA_DEMO_HPP */
