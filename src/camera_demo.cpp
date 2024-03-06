/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2021 PHYTEC Messtechnik GmbH
 */

#include <QtCore>
#include <QApplication>
#include <QQuickWindow>
#include <QImage>
#include <QPainter>
#include <QDebug>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <iostream>
#include <string>
#include <limits.h>
#include <fstream>
#include <unistd.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "camera_demo.hpp"

#include <json.hpp>
using json = nlohmann::json;

Sensor SENSORS[] = {
    {"", "", false, 0, 0, 0, 0, 0, 0},                          // default
    {"VM016", "ar0144", true, 1280, 800, 1280, 800, 0, 0},      // vm016
    {"VM017", "ar0521", false, 2560, 1440, 1280, 720, 16, 252}, // vm017
    {"VM020", "ar0234", true, 2560, 1440, 1280, 720, 16, 252},  // vm020
    {"---", "---", false, 0, 0, 0, 0, 0, 0}                     // Sentinel value to indicate the end

};

Host_hardware HOST_HARDWARE[] = {
    {"phyboard-pollux-imx8mp", 1, 1, 1, 1}, // phyBOARD-Pollux
    {"phyboard-polis-imx8mm", 0, 1, 0, 0},  // phyBOARD-Polis
    {"---", 0, 0, 0, 0}                     // Sentinel value to indicate the end
};

PhyCam::PhyCam(int _interface, Host_hardware *_host_hardware) : csi_interface(_interface), host_hardware(_host_hardware)
{
    // Check if camera device can be found in /dev
    device = "/dev/cam-csi" + std::to_string(csi_interface);
    if (access(device.c_str(), F_OK) == 0)
    { // phycam-M on csi-1
        port = -1;
    }
    else if (access((device + "-port0").c_str(), F_OK) == 0)
    { // phycam-L on csi-1, port0
        device += "-port0";
        port = 0;
    }
    else if (access((device + "-port1").c_str(), F_OK) == 0)
    { // phycam-L on csi-1, port1
        device += "-port1";
        port = 1;
    }
    else
    {
        status = UNCONNECTED;
        return;
    }

    // Check if ISP is available
    if (host_hardware->hasISP)
    {
        if ((access(("/dev/video-isp-csi" + std::to_string(csi_interface)).c_str(), F_OK) == 0))
        {
            ispAvailable = true;
        }
        else
        {
            ispAvailable = false;
        }
    }
    else
    {
        ispAvailable = false;
    }

    // Check if ISI is available
    if (host_hardware->hasISI)
    {
        if ((access(("/dev/video-isi-csi" + std::to_string(csi_interface)).c_str(), F_OK) == 0))
        {
            isiAvailable = true;
        }
        else
        {
            isiAvailable = false;
        }
    }
    else
    {
        isiAvailable = false;
    }
    // ---- Move this to CameraDemo Constrructor

    // Open device_fd
    device_fd = open(device.c_str(), O_RDWR);
    if (device_fd == -1)
    {
        std::cerr << "ERROR: Could not open device fd" << std::endl;
        status = ERROR;
        return;
    }
    // Open isp_fd
    if (ispAvailable)
    {
        isp_fd = open(("/dev/video-isp-csi" + std::to_string(csi_interface)).c_str(), O_RDWR | O_NONBLOCK, 0);
        if (isp_fd == -1)
        {
            std::cerr << "ERROR: Could not open isp fd" << std::endl;
            status = ERROR;
            return;
        }
    }
    // Get sensor of connected camera (includes framesize, format, pixelrate etc.)
    if (getSensor() < 0)
    {
        std::cerr << "ERROR: Failed to get sensor" << std::endl;
        status = ERROR;
        return;
    }
    // Construct setup_pipeline_command
    setup_pipeline_command = "/usr/bin/setup-pipeline-csi" + std::to_string(csi_interface);
    if (port == 1)
    {
        setup_pipeline_command += " -p 1";
    }
    setup_pipeline_command += " -c " + std::to_string(sensor->sensor_width) + "x" + std::to_string(sensor->sensor_height);
    setup_pipeline_command += " -s " + std::to_string(sensor->frame_width) + "x" + std::to_string(sensor->frame_height);
    setup_pipeline_command += " -o \"(" + std::to_string(sensor->offset_x) + "," + std::to_string(sensor->offset_y) + ")\"";

    // Construct gstreamer pipelines:
    std::string framesize = "width=" + std::to_string(sensor->frame_width) + ", height=" + std::to_string(sensor->frame_height);
    std::string isiFormat;
    std::string isiConversion;
    if (isColor)
    {
        isiFormat = "video/x-bayer,format=grbg, ";;
        isiConversion = " ! bayer2rgbneon";
    }
    else
    {
        isiFormat = "video/x-raw,format=GRAY8,depth=8, ";
    }
    if (host_hardware->hasDualCam)
    {
        isp_pipeline = "v4l2src device=/dev/video-isp-csi" + std::to_string(csi_interface) + " ! video/x-raw,format=YUY2, " + framesize + " ! videoconvert ! video/x-raw,format=RGB ! appsink name=mysink";
        isi_pipeline = "v4l2src device=/dev/video-isi-csi" + std::to_string(csi_interface) + " ! " + isiFormat + framesize + isiConversion + " ! videoconvert ! video/x-raw,format=RGB ! appsink name=mysink";
    }
    else
    {
        isi_pipeline = "v4l2src device=/dev/video-csi" + std::to_string(csi_interface) + " ! " + isiFormat + framesize + isiConversion + " ! videoconvert ! video/x-raw,format=RGB ! appsink name=mysink";
        isp_pipeline = "";
    }
    status = READY;
}
PhyCam::~PhyCam()
{
    close(device_fd);
    close(isp_fd);
}

int PhyCam::setup_pipeline()
{
    return system(setup_pipeline_command.c_str());
}

int PhyCam::getSensor()
{
    char v4l_subdev[PATH_MAX];
    ssize_t len = readlink(device.c_str(), v4l_subdev, sizeof(v4l_subdev) - 1);
    if (len != -1)
    {
        // get sensor name from sysfs
        v4l_subdev[len] = '\0';
        std::string entityPath = "/sys/class/video4linux/";
        entityPath += v4l_subdev;
        entityPath += "/device/name";
        std::ifstream entityFile(entityPath);
        std::string sensor_name;
        if (entityFile.is_open())
        {
            std::getline(entityFile, sensor_name);
            entityFile.close();
        }
        // find sensor in SENSORS
        for (int i = 0; SENSORS[i].name != "---"; i++)
        {
            if (SENSORS[i].name == sensor_name)
            {
                sensor = &SENSORS[i];

                // Get color format:
                struct v4l2_subdev_format fmt = {};
                fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
                fmt.pad = 0;
                if (ioctl(device_fd, VIDIOC_SUBDEV_G_FMT, &fmt) == -1)
                {
                    std::cerr << "ERROR: Failed to get format from subdev" << std::endl;
                    return errno;
                }
                if (fmt.format.code == MEDIA_BUS_FMT_Y8_1X8 || fmt.format.code == MEDIA_BUS_FMT_Y12_1X12)
                {
                    isColor = false;
                }
                else
                {
                    isColor = true;
                }

                // Get pixel rate (needed to calculate exposure time)
                // TODO Get pixel rate (somehow does not work with ioctl)
                // std::memset(&control, 0, sizeof(control));
                // control.id = V4L2_CID_PIXEL_RATE;
                // if (ioctl(CAM->device_fd, VIDIOC_G_CTRL, &control) == -1)
                // {
                //     std::cerr << "ERROR: Failed to get pixel rate control: " << strerror(errno) << std::endl;
                // }
                // int64_t pixel_rate = control.value;

                // TODO: workaround for getting pixel rate
                // Execute vl2-ctl command to get pixel rate
                QProcess process;
                QStringList arguments;
                arguments << "-d" << QString::fromStdString(device) << "-C"
                          << "pixel_rate";
                process.start("v4l2-ctl", arguments);
                process.waitForFinished(-1);
                QString output = process.readAllStandardOutput();
                // int returnCode = process.exitCode();
                // Extract the number from the output
                QString numberStr = output.split(":").last().trimmed();
                bool conversionOk;
                pixel_rate = numberStr.toInt(&conversionOk);
                // Check if conversion was successful
                if (!conversionOk)
                {
                    std::cerr << "ERROR: Unable to convert pixel rate to integer" << std::endl;
                    return 0;
                }

                return 0;
            }
        }
        std::cerr << "ERROR: Unknown sensor (not a phycam?)" << std::endl;
        return -1;
    }
    else
    {
        std::cerr << "ERROR: Could not read link" << std::endl;
        return -1;
    }
}

// Define a static callback function to forward the signal to the member function
static GstFlowReturn on_new_sample_callback(GstAppSink *sink, gpointer user_data) {
    // Cast the user data to the CameraDemo instance
    CameraDemo *cameraDemo = static_cast<CameraDemo *>(user_data);
    // Call the non-static member function using the instance
    return cameraDemo->on_new_sample(sink);
}

// Function to handle new Images
GstFlowReturn CameraDemo::on_new_sample(GstAppSink *sink)
{
    GstSample *sample = gst_app_sink_pull_sample(sink);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    int width = CAM->sensor->frame_width;
    int height = CAM->sensor->frame_height;

    QImage image =  QImage(map.data, width, height, QImage::Format_RGB888);
    emit newImage(image);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

void CameraDemo::startStream(std::string pipeline_string)
{
    stopStream();

    pipeline = gst_parse_launch(pipeline_string.c_str(), NULL);
    if (!pipeline)
    {
        std::cerr << "Failed to create pipeline" << std::endl;
    }

    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "mysink");
    if (!appsink)
    {
        std::cerr << "Failed to get appsink element" << std::endl;
        gst_object_unref(pipeline);
    }

    g_object_set(appsink, "emit-signals", TRUE, NULL);
    g_object_set(appsink, "caps", gst_caps_from_string("video/x-raw, format=RGB"), NULL); // set appsink to accept only RGB format
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample_callback), this);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);

    int ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        std::cerr << "Failed to set pipeline to playing state" << std::endl;
        gst_object_unref(pipeline);
        return;
    }
}
void CameraDemo::stopStream()
{
    if (pipeline)
    {
        gst_element_set_state(pipeline, GST_STATE_NULL);
    }
}


CameraDemo::CameraDemo(QObject *parent) : QObject(parent)
{
    gst_init(NULL, NULL);

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        std::string hostStr(hostname);

        for (int i = 0; HOST_HARDWARE[i].hostname != "---"; i++)
        {
            if (hostStr.find(HOST_HARDWARE[i].hostname) != std::string::npos)
            {
                host_hardware = &HOST_HARDWARE[i];
                emit hostHardwareChanged();
                if (host_hardware->hasDualCam)
                {

                    cam1 = new PhyCam(1, host_hardware);
                    cam2 = new PhyCam(2, host_hardware);
                    CAM = cam1;
                }
                else
                {
                    cam1 = new PhyCam(1, host_hardware);
                    cam2 = cam1;
                    CAM = cam1;
                }
                emit sensorChanged();
            }
            if (HOST_HARDWARE[i].hostname == "---")
            {
                std::cerr << "ERROR: Unknown hostname, your hardware is not supported by the qtphy demo" << std::endl;
                // exit(-1);
                // TBD: return to start page
            }
        }
    }
    else
    {
        std::cerr << "ERROR: Unable to get hostname" << std::endl;
        // exit(-1);
        // TBD: return to start page
    }

    // Check available cameras
    if (cam1->status == READY && cam2->status == READY)
    {
        CAM = cam1;
        host_hardware->dualCamAvailable = true;
    }
    else
    {
        host_hardware->dualCamAvailable = false;
        if (cam1->status == READY)
        {
            CAM = cam1;
        }
        else if (cam2->status == READY)
        {
            CAM = cam2;
        }
        detectCameras();
    }
    emit interfaceChanged();

    // Warn user when ISP or ISI is misconfigured (overlas not loaded)
    if (host_hardware->hasISP && ((cam1->status == READY && cam1->ispAvailable == false) ||
                                  (cam2->status == READY && cam2->ispAvailable == false)))
    {
        if (RECOMMENDED_OVERLAYS.isEmpty())
        {
            detectCameras();
        }
        STATUS = ISP_UNAVAILABLE;
        emit statusChanged();
    }
    if (host_hardware->hasISI && ((cam1->status == READY && cam1->isiAvailable == false) ||
                                  (cam2->status == READY && cam2->isiAvailable == false)))
    {
        if (RECOMMENDED_OVERLAYS.isEmpty())
        {
            detectCameras();
        }
        STATUS = ISI_UNAVAILABLE;
        emit statusChanged();
    }

    // select video source
    if (CAM->ispAvailable)
    {
        CAM->video_src = ISP;
    }
    else
    {
        CAM->video_src = ISI;
    }
    emit videoSrcChanged();
}

CameraDemo::~CameraDemo()
{
    stopStream();
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    gst_object_unref(appsink);
    if (cam2 == cam1)
    {
        delete cam1;
    }
    else
    {
        delete cam1;
        delete cam2;
    }
}

void CameraDemo::detectCameras()
{
    // Execute detectCamera script to check if additional cameras are connected
    QProcess process;
    QStringList arguments;
    arguments << "detectCamera"
              << "-m";
    process.start("/bin/sh", arguments);
    process.waitForFinished(-1);
    QString output = process.readAllStandardOutput();
    int returnCode = process.exitCode();
    RECOMMENDED_OVERLAYS = output;
    emit recommendedOverlaysChanged();

    if (returnCode == 0)
    {
        // Additional cameras found (reload overlays to use them)
        STATUS = WRONG_OVERLAYS;
        emit statusChanged();
        return;
    }
    // else if (returnCode == 1) // no additional camera found
    else if (returnCode == 2)
    {
        // No camera connected
        STATUS = NO_CAM;
    }
}

int CameraDemo::isp_ioctl(const char *cmd, json &jsonRequest, json &jsonResponse)
{
    if (CAM->isp_fd < 0)
    {
        std::cerr << "ERROR: ISP file descriptor < 0" << std::endl;
        return -1;
    }
    if (!cmd)
    {
        return -1;
    }

    jsonRequest["id"] = cmd;
    jsonRequest["streamid"] = 0;

    struct v4l2_ext_controls ecs;
    struct v4l2_ext_control ec;
    memset(&ecs, 0, sizeof(ecs));
    memset(&ec, 0, sizeof(ec));
    ec.string = new char[64 * 1024];
    ec.id = V4L2_CID_VIV_EXTCTRL;
    ec.size = 0;
    ecs.controls = &ec;
    ecs.count = 1;

    ioctl(CAM->isp_fd, VIDIOC_G_EXT_CTRLS, &ecs);

    // --- Initialized --- //

    strcpy(ec.string, jsonRequest.dump().c_str());

    // Set V4L2-control
    int ret = ioctl(CAM->isp_fd, VIDIOC_S_EXT_CTRLS, &ecs);
    if (ret != 0)
    {
        std::cerr << "ERROR: Failed to set ISP ext ctrl\n"
                  << std::endl;
        delete[] ec.string;
        ec.string = NULL;
        return 555;
    }
    else
    {
        // Get V4L2-control
        ioctl(CAM->isp_fd, VIDIOC_G_EXT_CTRLS, &ecs);

        std::string res = ec.string;
        jsonResponse = json::parse(res);
        delete[] ec.string;
        ec.string = NULL;
        return 0;
    }
}

void CameraDemo::setDwe(bool value)
{
    json jRequest, jResponse;
    jRequest["bypass"] = !value;
    isp_ioctl("dwe.s.bypass", jRequest, jResponse);
    return;
}

void CameraDemo::setAwb(bool value)
{
    // Enable AWB
    json jRequest, jResponse;
    jRequest["enable"] = value;
    isp_ioctl("awb.s.en", jRequest, jResponse);

    // Configure AWB parameters
    json request = json::parse(R"(
        {
        "matrix": [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0],
        "offset": {
            "blue": 0,
            "green": 0,
            "red": 0
        },
        "wb.gains": {
            "blue": 1.0,
            "green.b": 1.0,
            "green.r": 1.0,
            "red": "red"
        }
        }
    )");
    isp_ioctl("wb.s.cfg", jRequest, jResponse);
    return;
}

void CameraDemo::setLsc(bool value)
{
    json jRequest, jResponse;
    jRequest["enable"] = value;
    isp_ioctl("lsc.s.en", jRequest, jResponse);
    return;
}

void CameraDemo::setAec(bool value)
{
    json jRequest, jResponse;
    jRequest["enable"] = value;
    isp_ioctl("ae.s.en", jRequest, jResponse);
    return;
}

void CameraDemo::openCamera()
{
    if (cam1->status == ACTIVE || cam2->status == ACTIVE)
    {
        return;
    }

    // Start capturing video
    if (CAM->video_src == ISP)
    {
        startStream(CAM->isp_pipeline);
    }
    else
    {
        startStream(CAM->isi_pipeline);
    }

    CAM->status = ACTIVE;
}

void CameraDemo::reloadOverlays()
{
    std::string command = "/root/detectCamera.sh -s \"" + RECOMMENDED_OVERLAYS.toStdString() + "\"";
    system(command.c_str());
}

// ################# OpencvImageProvider #################
OpencvImageProvider::OpencvImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    image = QImage(1280, 800, QImage::Format_RGB888);
    image.fill(QColor("blue"));
}

QImage OpencvImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id);

    if (size)
    {
        *size = image.size();
    }

    if (requestedSize.width() > 0 && requestedSize.height() > 0)
    {
        image = image.scaled(requestedSize.width(), requestedSize.height(), Qt::KeepAspectRatio);
    }
    return image;
}

void OpencvImageProvider::updateImage(const QImage image)
{
    this->image = image;
    emit imageChanged();
}

// ################# GETTER FUNCTIONS FOR UI #################
QString CameraDemo::getCameraName() const
{
    std::string ret = CAM->sensor->camera_name + " (" + CAM->sensor->name + ")";
    return QString::fromStdString(ret);
}
QString CameraDemo::getFramesize() const
{
    std::string ret = std::to_string(CAM->sensor->frame_width) + "x" + std::to_string(CAM->sensor->frame_height);
    return QString::fromStdString(ret);
}
QString CameraDemo::getInterfaceString() const
{
    std::string ret = "CSI" + std::to_string(CAM->csi_interface);
    if (CAM->port >= 0)
    {
        ret += ", PORT" + std::to_string(CAM->port);
    }
    return QString::fromStdString(ret);
}
int CameraDemo::getInterface() const
{
    return CAM->csi_interface;
}
int CameraDemo::getVideoSrc() const
{
    return CAM->video_src;
}
bool CameraDemo::getIspAvailable() const
{
    return CAM->ispAvailable;
}
bool CameraDemo::getIsiAvailable() const
{
    return CAM->isiAvailable;
}

QString CameraDemo::getRecommendedOverlays() const
{
    return RECOMMENDED_OVERLAYS;
}

Status CameraDemo::getStatus()
{
    return STATUS;
}

bool CameraDemo::getAutoExposure()
{
    if (CAM->device_fd < 0)
    {
        return false;
    }
    if (!CAM->sensor->hasAutoExposure)
    {
        return 0;
    }
    struct v4l2_control control;
    std::memset(&control, 0, sizeof(control));

    control.id = V4L2_CID_EXPOSURE_AUTO;
    if (ioctl(CAM->device_fd, VIDIOC_G_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: getting exposure mode" << std::endl;
    }

    if (control.value == V4L2_EXPOSURE_AUTO)
    {
        return true;
    }
    else if (control.value == V4L2_EXPOSURE_MANUAL)
    {
        return false;
    }
    else
    {
        std::cerr << "ERROR: Unknown exposure mode" << std::endl;
        return false;
    }
}

bool CameraDemo::getHasAutoExposure()
{
    return CAM->sensor->hasAutoExposure;
}

bool CameraDemo::getFlipHorizontal()
{
    if (CAM->device_fd < 0)
    {
        return false;
    }
    struct v4l2_control control;
    std::memset(&control, 0, sizeof(control));

    control.id = V4L2_CID_HFLIP;
    if (ioctl(CAM->device_fd, VIDIOC_G_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: getting horizontal flip" << std::endl;
    }

    return control.value;
}

bool CameraDemo::getFlipVertical()
{
    if (CAM->device_fd < 0)
    {
        return false;
    }
    struct v4l2_control control;
    std::memset(&control, 0, sizeof(control));

    control.id = V4L2_CID_VFLIP;
    if (ioctl(CAM->device_fd, VIDIOC_G_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: getting vertical flip" << std::endl;
    }

    return control.value;
}

int CameraDemo::getExposure()
{
    if (CAM->device_fd < 0)
    {
        return 0;
    }

    struct v4l2_control control;

    // Get exposure
    std::memset(&control, 0, sizeof(control));
    control.id = V4L2_CID_EXPOSURE;
    if (ioctl(CAM->device_fd, VIDIOC_G_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: getting exposure" << std::endl;
    }

    // std::cout << "Got exposure time: " << (int64_t)CAM->sensor->frame_width * (int64_t)control.value * 1000000 / CAM->pixel_rate << std::endl;
    return (int64_t)CAM->sensor->frame_width * (int64_t)control.value * 1000000 / CAM->pixel_rate; // exposure time in microseconds
}

Host_hardware CameraDemo::getHostHardware()
{
    return *host_hardware;
}

// ################# SLOTS (Called from UI) #################
void CameraDemo::setVideoSource(Video_srcs value)
{
    stopStream();
    if (value == ISP)
    {
        CAM->video_src = ISP;
        if (CAM->status == ACTIVE)
        {
            startStream(CAM->isp_pipeline);
        }
        setAutoExposure(false); // use ISP auto exposure (disable sensor auto exposure)
    }
    else if (value == ISI)
    {
        CAM->video_src = ISI;
        CAM->setup_pipeline(); // setup pipeline every time ISP is switched to ISI
        if (CAM->status == ACTIVE)
        {
            startStream(CAM->isi_pipeline);
        }
    }
    emit videoSrcChanged();
}

void CameraDemo::setInterface(CSI_interface value)
{
    Camera_status previousCamStatus = CAM->status;

    if (CAM->status == ACTIVE)
    {
        stopStream();
        CAM->status = READY;
    }

    if (value == CSI1 && cam1->status == READY)
    {
        CAM = cam1;
    }
    else if (value == CSI2 && cam2->status == READY)
    {
        CAM = cam2;
    }
    else
    {
        std::cerr << "ERROR: Camera not ready" << std::endl;
        return;
    }

    CAM->setup_pipeline();
    // sleep(1);

    if (previousCamStatus == ACTIVE)
    {
        if (CAM->video_src == ISP)
        {
            startStream(CAM->isp_pipeline);
        }
        else
        {
            startStream(CAM->isi_pipeline);
        }
        CAM->status = ACTIVE;
    }
    emit interfaceChanged();
    emit sensorChanged();
    emit videoSrcChanged();
    emit autoExposureChanged();
    emit flipVerticalChanged();
    emit flipHorizontalChanged();
    emit exposureChanged();
}

void CameraDemo::setAutoExposure(bool value)
{
    if (!CAM->sensor->hasAutoExposure)
    {
        std::cerr << "ERROR: This camera has no auto exposure" << std::endl;
        return;
    }
    struct v4l2_control control;
    control.id = V4L2_CID_EXPOSURE_AUTO;
    if (value)
    {
        control.value = V4L2_EXPOSURE_AUTO;
    }
    else
    {
        control.value = V4L2_EXPOSURE_MANUAL;
        emit exposureChanged();
    }

    if (ioctl(CAM->device_fd, VIDIOC_S_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: Can't set auto exposure" << std::endl;
    }
    emit autoExposureChanged();
}

void CameraDemo::setExposure(int value)
{
    struct v4l2_control control;
    control.id = V4L2_CID_EXPOSURE;
    control.value = value * CAM->pixel_rate / CAM->sensor->frame_width / 1000000; // calculate exposure time in lines

    // Set exposure
    if (ioctl(CAM->device_fd, VIDIOC_S_CTRL, &control) == -1)
    {
        std::cerr << ("ERROR: Can't set exposure") << std::endl;
    }
}

void CameraDemo::setFlipVertical(bool value)
{
    struct v4l2_control control;
    control.id = V4L2_CID_VFLIP;
    control.value = value;

    if (ioctl(CAM->device_fd, VIDIOC_S_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: Can't set vertical flip" << std::endl;
    }
}

void CameraDemo::setFlipHorizontal(bool value)
{
    struct v4l2_control control;
    control.id = V4L2_CID_HFLIP;
    control.value = value;

    if (ioctl(CAM->device_fd, VIDIOC_S_CTRL, &control) == -1)
    {
        std::cerr << "ERROR: Can't set horizontal flip" << std::endl;
    }
}