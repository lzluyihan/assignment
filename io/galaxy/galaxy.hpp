#ifndef IO__GALAXY__GALAXY_HPP
#define IO__GALAXY__GALAXY_HPP

#include <chrono>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>

#include "../../tools/thread_safe_queue.hpp"
#include "../camera.hpp"
#include "include/GxIAPI.h"

namespace io {

class Galaxy : public CameraBase {
public:
  Galaxy(double exposure_ms, double gain, const std::string &vid_pid);
  ~Galaxy() override;

  void read(cv::Mat &img,
            std::chrono::steady_clock::time_point &timestamp) override;

private:
  struct CameraData {
    cv::Mat img;
    std::chrono::steady_clock::time_point timestamp;
  };

  void initializeLibrary();
  void openDevice(const std::string &vid_pid);
  void configureCamera(double exposure_ms, double gain);
  void startAcquisition();
  void stopAcquisition();
  void closeDevice();
  bool convertFrameToMat(PGX_FRAME_BUFFER frame_buffer, cv::Mat &img);

  void captureLoop();

  GX_DEV_HANDLE device_handle_;
  bool is_open_;
  bool is_streaming_;

  std::thread capture_thread_;
  std::atomic<bool> capture_thread_running_;
  tools::ThreadSafeQueue<CameraData> frame_queue_;
};

} // namespace io

#endif // IO__GALAXY__GALAXY_HPP