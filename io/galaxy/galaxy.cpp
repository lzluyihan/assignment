#include "galaxy.hpp"
#include "opencv2/opencv.hpp"
#include <iostream>
#include <stdexcept>
// #include <thread>

namespace io {

Galaxy::Galaxy(double exposure_ms, double gain, const std::string &vid_pid)
    : device_handle_(nullptr), is_open_(false), is_streaming_(false),
      capture_thread_running_(false), frame_queue_(3) { // 队列大小为3帧
  try {
    initializeLibrary();
    openDevice(vid_pid);
    configureCamera(exposure_ms, gain);
    startAcquisition();

    // 启动采集线程
    capture_thread_running_ = true;
    capture_thread_ = std::thread(&Galaxy::captureLoop, this);
    std::cout << "Capture thread started" << std::endl;
  } catch (const std::exception &e) {
    if (is_streaming_) {
      stopAcquisition();
    }
    if (is_open_) {
      closeDevice();
    }
    GXCloseLib();
    throw;
  }
}

Galaxy::~Galaxy() {
  // 停止采集线程
  capture_thread_running_ = false;
  if (capture_thread_.joinable()) {
    capture_thread_.join();
    std::cout << "Capture thread stopped" << std::endl;
  }

  if (is_streaming_) {
    stopAcquisition();
  }
  if (is_open_) {
    closeDevice();
  }
  GXCloseLib();
}

void Galaxy::initializeLibrary() {
  GX_STATUS status = GXInitLib();
  if (status != GX_STATUS_SUCCESS) {
    throw std::runtime_error("Failed to initialize Galaxy library, status: " +
                             std::to_string(status));
  }
  std::cout << "Galaxy library initialized successfully" << std::endl;
}

void Galaxy::openDevice(const std::string &vid_pid) {
  uint32_t device_num = 0;

  // 枚举设备
  GX_STATUS status = GXUpdateDeviceList(&device_num, 1000);
  if (status != GX_STATUS_SUCCESS) {
    throw std::runtime_error("Failed to enumerate Galaxy devices, status: " +
                             std::to_string(status));
  }

  std::cout << "Found " << device_num << " Galaxy device(s)" << std::endl;

  if (device_num == 0) {
    throw std::runtime_error("No Galaxy camera found");
  }

  // 如果指定了vid_pid且不为空，尝试根据序列号打开特定设备
  if (!vid_pid.empty()) {
    // 获取设备信息列表
    GX_DEVICE_BASE_INFO *device_info = new GX_DEVICE_BASE_INFO[device_num];
    size_t info_size = device_num * sizeof(GX_DEVICE_BASE_INFO);

    status = GXGetAllDeviceBaseInfo(device_info, &info_size);
    if (status == GX_STATUS_SUCCESS) {
      // 查找匹配的设备
      bool found = false;
      for (uint32_t i = 0; i < device_num; i++) {
        std::string device_sn(device_info[i].szSN);
        std::cout << "Device " << i << " SN: " << device_sn << std::endl;
        if (device_sn.find(vid_pid) != std::string::npos) {
          status = GXOpenDeviceByIndex(i + 1, &device_handle_);
          found = true;
          std::cout << "Opening device by SN match" << std::endl;
          break;
        }
      }
      if (!found) {
        std::cout << "Device with SN containing '" << vid_pid
                  << "' not found, opening first device" << std::endl;
        status = GXOpenDeviceByIndex(1, &device_handle_);
      }
    }
    delete[] device_info;
  } else {
    // 打开第一个设备
    std::cout << "Opening first available device" << std::endl;
    status = GXOpenDeviceByIndex(1, &device_handle_);
  }

  if (status != GX_STATUS_SUCCESS) {
    throw std::runtime_error("Failed to open Galaxy camera, status: " +
                             std::to_string(status));
  }

  is_open_ = true;
  std::cout << "Galaxy camera opened successfully" << std::endl;
}

// void Galaxy::configureCamera(double exposure_ms, double gain) {
//   GX_STATUS status;
//   bool is_implemented = false;

//   std::cout << "Configuring camera with exposure: " << exposure_ms
//             << "ms, gain: " << gain << std::endl;

//   // 设置触发模式为OFF（连续采集）
//   status =
//       GXIsImplemented(device_handle_, GX_ENUM_TRIGGER_MODE, &is_implemented);
//   if (status == GX_STATUS_SUCCESS && is_implemented) {
//     status =
//         GXSetEnum(device_handle_, GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_OFF);
//     if (status != GX_STATUS_SUCCESS) {
//       std::cerr << "Warning: Failed to set trigger mode, status: " << status
//                 << std::endl;
//     } else {
//       std::cout << "Trigger mode set to OFF" << std::endl;
//     }
//   }

//   // 设置采集模式为连续
//   status = GXIsImplemented(device_handle_, GX_ENUM_ACQUISITION_MODE,
//                            &is_implemented);
//   if (status == GX_STATUS_SUCCESS && is_implemented) {
//     status = GXSetEnum(device_handle_, GX_ENUM_ACQUISITION_MODE,
//                        GX_ACQ_MODE_CONTINUOUS);
//     if (status != GX_STATUS_SUCCESS) {
//       std::cerr << "Warning: Failed to set acquisition mode, status: " <<
//       status
//                 << std::endl;
//       // 尝试其他采集模式
//       status = GXSetEnum(device_handle_, GX_ENUM_ACQUISITION_MODE,
//                          GX_ACQ_MODE_SINGLE_FRAME);
//       if (status == GX_STATUS_SUCCESS) {
//         std::cout << "Set to single frame mode instead" << std::endl;
//       }
//     } else {
//       std::cout << "Acquisition mode set to continuous" << std::endl;
//     }
//   }

//   // 检查并设置像素格式
//   status =
//       GXIsImplemented(device_handle_, GX_ENUM_PIXEL_FORMAT, &is_implemented);
//   if (status == GX_STATUS_SUCCESS && is_implemented) {
//     // 首先尝试设置为BGR8
//     status =
//         GXSetEnum(device_handle_, GX_ENUM_PIXEL_FORMAT,
//         GX_PIXEL_FORMAT_BGR8);
//     if (status != GX_STATUS_SUCCESS) {
//       // 如果BGR8不支持，尝试设置为Bayer格式
//       status = GXSetEnum(device_handle_, GX_ENUM_PIXEL_FORMAT,
//                          GX_PIXEL_FORMAT_BAYER_GR8);
//       if (status != GX_STATUS_SUCCESS) {
//         // 如果还是不行，尝试MONO8
//         status = GXSetEnum(device_handle_, GX_ENUM_PIXEL_FORMAT,
//                            GX_PIXEL_FORMAT_MONO8);
//         if (status == GX_STATUS_SUCCESS) {
//           std::cout << "Pixel format set to MONO8" << std::endl;
//         }
//       } else {
//         std::cout << "Pixel format set to Bayer GR8" << std::endl;
//       }
//     } else {
//       std::cout << "Pixel format set to BGR8" << std::endl;
//     }
//   }

//   // 设置曝光时间 (微秒)
//   double exposure_us = exposure_ms * 1000.0;
//   status =
//       GXIsImplemented(device_handle_, GX_FLOAT_EXPOSURE_TIME,
//       &is_implemented);
//   if (status == GX_STATUS_SUCCESS && is_implemented) {
//     // 获取曝光时间范围
//     GX_FLOAT_RANGE exposure_range;
//     status = GXGetFloatRange(device_handle_, GX_FLOAT_EXPOSURE_TIME,
//                              &exposure_range);
//     if (status == GX_STATUS_SUCCESS) {
//       std::cout << "Exposure range: " << exposure_range.dMin << " - "
//                 << exposure_range.dMax << " us" << std::endl;
//       // 限制曝光时间在有效范围内
//       exposure_us = std::max(exposure_range.dMin,
//                              std::min(exposure_range.dMax, exposure_us));
//       status = GXSetFloat(device_handle_, GX_FLOAT_EXPOSURE_TIME,
//       exposure_us); if (status != GX_STATUS_SUCCESS) {
//         std::cerr << "Warning: Failed to set exposure time, status: " <<
//         status
//                   << std::endl;
//       } else {
//         std::cout << "Exposure time set to " << exposure_us << " us"
//                   << std::endl;
//       }
//     }
//   }

//   // 设置增益
//   status = GXIsImplemented(device_handle_, GX_FLOAT_GAIN, &is_implemented);
//   if (status == GX_STATUS_SUCCESS && is_implemented) {
//     // 获取增益范围
//     GX_FLOAT_RANGE gain_range;
//     status = GXGetFloatRange(device_handle_, GX_FLOAT_GAIN, &gain_range);
//     if (status == GX_STATUS_SUCCESS) {
//       std::cout << "Gain range: " << gain_range.dMin << " - " <<
//       gain_range.dMax
//                 << std::endl;
//       // 限制增益在有效范围内
//       gain = std::max(gain_range.dMin, std::min(gain_range.dMax, gain));
//       status = GXSetFloat(device_handle_, GX_FLOAT_GAIN, gain);
//       if (status != GX_STATUS_SUCCESS) {
//         std::cerr << "Warning: Failed to set gain, status: " << status
//                   << std::endl;
//       } else {
//         std::cout << "Gain set to " << gain << std::endl;
//       }
//     }
//   }

//   std::cout << "Camera configuration completed" << std::endl;
// }

void Galaxy::configureCamera(double exposure_ms, double gain) {
  GX_STATUS status;
  bool is_implemented = false;

  std::cout << "Configuring camera with exposure: " << exposure_ms
            << "ms, gain: " << gain << std::endl;

  // 设置触发模式为OFF(连续采集)
  status =
      GXIsImplemented(device_handle_, GX_ENUM_TRIGGER_MODE, &is_implemented);
  if (status == GX_STATUS_SUCCESS && is_implemented) {
    status =
        GXSetEnum(device_handle_, GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_OFF);
    if (status != GX_STATUS_SUCCESS) {
      std::cerr << "Warning: Failed to set trigger mode, status: " << status
                << std::endl;
    } else {
      std::cout << "Trigger mode set to OFF" << std::endl;
    }
  }

  // 设置采集模式为连续
  status = GXIsImplemented(device_handle_, GX_ENUM_ACQUISITION_MODE,
                           &is_implemented);
  if (status == GX_STATUS_SUCCESS && is_implemented) {
    status = GXSetEnum(device_handle_, GX_ENUM_ACQUISITION_MODE,
                       GX_ACQ_MODE_CONTINUOUS);
    if (status != GX_STATUS_SUCCESS) {
      std::cerr << "Warning: Failed to set acquisition mode, status: " << status
                << std::endl;
    } else {
      std::cout << "Acquisition mode set to continuous" << std::endl;
    }
  }

  // 关闭帧率控制模式,使用最大帧率
  status = GXIsImplemented(device_handle_, GX_ENUM_ACQUISITION_FRAME_RATE_MODE,
                           &is_implemented);
  if (status == GX_STATUS_SUCCESS && is_implemented) {
    status = GXSetEnum(device_handle_, GX_ENUM_ACQUISITION_FRAME_RATE_MODE,
                       GX_ACQUISITION_FRAME_RATE_MODE_OFF);
    if (status == GX_STATUS_SUCCESS) {
      std::cout << "Frame rate control mode disabled (maximum frame rate)"
                << std::endl;
    } else {
      std::cerr << "Warning: Failed to disable frame rate control, status: "
                << status << std::endl;
    }
  }

  // 如果需要设置固定帧率,可以使用以下代码(注释掉则使用最大帧率)
  /*
  status = GXIsImplemented(device_handle_, GX_FLOAT_ACQUISITION_FRAME_RATE,
  &is_implemented); if (status == GX_STATUS_SUCCESS && is_implemented) { double
  target_fps = 120.0;  // 目标帧率 status = GXSetFloat(device_handle_,
  GX_FLOAT_ACQUISITION_FRAME_RATE, target_fps); if (status == GX_STATUS_SUCCESS)
  { std::cout << "Target frame rate set to " << target_fps << " fps" <<
  std::endl;
    }
  }
  */

  // 检查并设置像素格式 - 优先使用 BGR8 以避免转换开销
  status =
      GXIsImplemented(device_handle_, GX_ENUM_PIXEL_FORMAT, &is_implemented);
  if (status == GX_STATUS_SUCCESS && is_implemented) {
    // 首先尝试设置为BGR8(直接输出,无需转换)
    status =
        GXSetEnum(device_handle_, GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_BGR8);
    if (status != GX_STATUS_SUCCESS) {
      // 如果BGR8不支持,尝试RGB8
      status =
          GXSetEnum(device_handle_, GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_RGB8);
      if (status != GX_STATUS_SUCCESS) {
        // 最后尝试Bayer格式(需要转换)
        status = GXSetEnum(device_handle_, GX_ENUM_PIXEL_FORMAT,
                           GX_PIXEL_FORMAT_BAYER_RG8);
        if (status == GX_STATUS_SUCCESS) {
          std::cout << "Pixel format set to BAYER_RG8 (requires conversion)"
                    << std::endl;
        }
      } else {
        std::cout << "Pixel format set to RGB8" << std::endl;
      }
    } else {
      std::cout << "Pixel format set to BGR8 (optimal)" << std::endl;
    }
  }

  // 设置曝光时间(微秒)
  double exposure_us = exposure_ms * 1000.0;
  status =
      GXIsImplemented(device_handle_, GX_FLOAT_EXPOSURE_TIME, &is_implemented);
  if (status == GX_STATUS_SUCCESS && is_implemented) {
    GX_FLOAT_RANGE exposure_range;
    status = GXGetFloatRange(device_handle_, GX_FLOAT_EXPOSURE_TIME,
                             &exposure_range);
    if (status == GX_STATUS_SUCCESS) {
      std::cout << "Exposure range: " << exposure_range.dMin << " - "
                << exposure_range.dMax << " us" << std::endl;
      exposure_us = std::max(exposure_range.dMin,
                             std::min(exposure_range.dMax, exposure_us));
      status = GXSetFloat(device_handle_, GX_FLOAT_EXPOSURE_TIME, exposure_us);
      if (status != GX_STATUS_SUCCESS) {
        std::cerr << "Warning: Failed to set exposure time, status: " << status
                  << std::endl;
      } else {
        std::cout << "Exposure time set to " << exposure_us << " us"
                  << std::endl;
      }
    }
  }

  // 设置增益
  status = GXIsImplemented(device_handle_, GX_FLOAT_GAIN, &is_implemented);
  if (status == GX_STATUS_SUCCESS && is_implemented) {
    GX_FLOAT_RANGE gain_range;
    status = GXGetFloatRange(device_handle_, GX_FLOAT_GAIN, &gain_range);
    if (status == GX_STATUS_SUCCESS) {
      std::cout << "Gain range: " << gain_range.dMin << " - " << gain_range.dMax
                << std::endl;
      gain = std::max(gain_range.dMin, std::min(gain_range.dMax, gain));
      status = GXSetFloat(device_handle_, GX_FLOAT_GAIN, gain);
      if (status != GX_STATUS_SUCCESS) {
        std::cerr << "Warning: Failed to set gain, status: " << status
                  << std::endl;
      } else {
        std::cout << "Gain set to " << gain << std::endl;
      }
    }
  }

  std::cout << "Camera configuration completed" << std::endl;
}

void Galaxy::startAcquisition() {
  std::cout << "Starting acquisition..." << std::endl;

  GX_STATUS status = GXStreamOn(device_handle_);
  if (status != GX_STATUS_SUCCESS) {
    throw std::runtime_error("Failed to start stream, status: " +
                             std::to_string(status));
  }
  is_streaming_ = true;
  std::cout << "Acquisition started successfully" << std::endl;
}

void Galaxy::stopAcquisition() {
  if (is_streaming_) {
    std::cout << "Stopping acquisition..." << std::endl;
    GX_STATUS status = GXStreamOff(device_handle_);
    if (status != GX_STATUS_SUCCESS) {
      std::cerr << "Warning: Failed to stop stream, status: " << status
                << std::endl;
    } else {
      std::cout << "Acquisition stopped" << std::endl;
    }
    is_streaming_ = false;
  }
}

void Galaxy::closeDevice() {
  if (is_open_ && device_handle_ != nullptr) {
    std::cout << "Closing device..." << std::endl;
    GX_STATUS status = GXCloseDevice(device_handle_);
    if (status != GX_STATUS_SUCCESS) {
      std::cerr << "Warning: Failed to close device, status: " << status
                << std::endl;
    } else {
      std::cout << "Device closed" << std::endl;
    }
    device_handle_ = nullptr;
    is_open_ = false;
  }
}

void Galaxy::captureLoop() {
  std::cout << "Capture loop started" << std::endl;

  while (capture_thread_running_) {
    if (!is_streaming_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    try {
      PGX_FRAME_BUFFER frame_buffer = nullptr;
      const int timeout_ms = 100; // 100ms超时，避免阻塞太久

      // 获取图像帧
      GX_STATUS status = GXDQBuf(device_handle_, &frame_buffer, timeout_ms);
      if (status == GX_STATUS_TIMEOUT) {
        continue; // 超时继续下一次循环
      }
      if (status != GX_STATUS_SUCCESS) {
        std::cerr << "Failed to grab image in capture loop, status: " << status
                  << std::endl;
        continue;
      }

      // 检查帧状态
      if (frame_buffer->nStatus != GX_FRAME_STATUS_SUCCESS) {
        GXQBuf(device_handle_, frame_buffer);
        continue;
      }

      // 在采集线程中进行Bayer转换（最耗时的部分）
      cv::Mat img;
      auto timestamp = std::chrono::steady_clock::now();
      bool success = convertFrameToMat(frame_buffer, img);

      // 归还帧缓存（尽快释放SDK缓冲区）
      GXQBuf(device_handle_, frame_buffer);

      if (success && !img.empty()) {
        // 将转换好的图像放入队列（使用移动语义，避免拷贝）
        CameraData camera_data{std::move(img), timestamp};
        frame_queue_.push(std::move(camera_data));
      }

    } catch (const std::exception &e) {
      std::cerr << "Exception in capture loop: " << e.what() << std::endl;
    }
  }

  std::cout << "Capture loop ended" << std::endl;
}

void Galaxy::read(cv::Mat &img,
                  std::chrono::steady_clock::time_point &timestamp) {
  if (!is_streaming_) {
    throw std::runtime_error("Camera is not streaming");
  }

  // 从队列中取出已转换好的图像（阻塞等待）
  CameraData camera_data;
  frame_queue_.pop(camera_data);

  img = std::move(camera_data.img);
  timestamp = camera_data.timestamp;
}

bool Galaxy::convertFrameToMat(PGX_FRAME_BUFFER frame_buffer, cv::Mat &img) {
  if (frame_buffer == nullptr || frame_buffer->pImgBuf == nullptr) {
    return false;
  }

  int width = static_cast<int>(frame_buffer->nWidth);
  int height = static_cast<int>(frame_buffer->nHeight);

  // 添加调试信息 - 只在第一次打印
  static bool first_time = true;
  if (first_time) {
    std::cout << "=== Camera Debug Info ===" << std::endl;
    std::cout << "Image size: " << width << "x" << height << std::endl;
    std::cout << "Pixel format: " << frame_buffer->nPixelFormat << std::endl;

    // 打印具体的像素格式名称
    switch (frame_buffer->nPixelFormat) {
    case GX_PIXEL_FORMAT_MONO8:
      std::cout << "Format name: MONO8" << std::endl;
      break;
    case GX_PIXEL_FORMAT_BGR8:
      std::cout << "Format name: BGR8" << std::endl;
      break;
    case GX_PIXEL_FORMAT_RGB8:
      std::cout << "Format name: RGB8" << std::endl;
      break;
    case GX_PIXEL_FORMAT_BAYER_GR8:
      std::cout << "Format name: BAYER_GR8" << std::endl;
      break;
    case GX_PIXEL_FORMAT_BAYER_RG8:
      std::cout << "Format name: BAYER_RG8" << std::endl;
      break;
    case GX_PIXEL_FORMAT_BAYER_GB8:
      std::cout << "Format name: BAYER_GB8" << std::endl;
      break;
    case GX_PIXEL_FORMAT_BAYER_BG8:
      std::cout << "Format name: BAYER_BG8" << std::endl;
      break;
    default:
      std::cout << "Format name: UNKNOWN (" << frame_buffer->nPixelFormat << ")"
                << std::endl;
      break;
    }
    std::cout << "=========================" << std::endl;
    first_time = false;
  }

  // 根据像素格式处理
  switch (frame_buffer->nPixelFormat) {
  case GX_PIXEL_FORMAT_MONO8:
    img = cv::Mat(height, width, CV_8UC1, frame_buffer->pImgBuf).clone();
    break;

  case GX_PIXEL_FORMAT_BGR8:
    img = cv::Mat(height, width, CV_8UC3, frame_buffer->pImgBuf).clone();
    break;

  case GX_PIXEL_FORMAT_RGB8: {
    cv::Mat rgb_img(height, width, CV_8UC3, frame_buffer->pImgBuf);
    cv::cvtColor(rgb_img, img, cv::COLOR_RGB2BGR);
  } break;

  case GX_PIXEL_FORMAT_BAYER_GR8: {
    cv::Mat bayer_img(height, width, CV_8UC1, frame_buffer->pImgBuf);
    cv::cvtColor(bayer_img, img, cv::COLOR_BayerGR2BGR_VNG);
  } break;

  case GX_PIXEL_FORMAT_BAYER_RG8: {
    cv::Mat bayer_img(height, width, CV_8UC1, frame_buffer->pImgBuf);
    // 尝试不同的Bayer转换模式
    cv::cvtColor(bayer_img, img,
                 cv::COLOR_BayerBG2BGR_EA); // 改为BG模式适配大恒
  } break;

  case GX_PIXEL_FORMAT_BAYER_GB8: {
    cv::Mat bayer_img(height, width, CV_8UC1, frame_buffer->pImgBuf);
    cv::cvtColor(bayer_img, img, cv::COLOR_BayerGB2BGR);
  } break;

  case GX_PIXEL_FORMAT_BAYER_BG8: {
    cv::Mat bayer_img(height, width, CV_8UC1, frame_buffer->pImgBuf);
    cv::cvtColor(bayer_img, img, cv::COLOR_BayerBG2BGR);
  } break;

  default:
    std::cerr << "Unsupported pixel format: " << frame_buffer->nPixelFormat
              << std::endl;
    return false;
  }

  return true;
}

} // namespace io