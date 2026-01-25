#ifndef IO__GIMBAL_HPP
#define IO__GIMBAL_HPP

#include <Eigen/Geometry>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

#include "serial/serial.h"
#include "tools/thread_safe_queue.hpp"

namespace io
{
struct __attribute__((packed)) GimbalToVision
{
  uint8_t head = 0xff;
  uint8_t mode;  //云台工作模式  0: 空闲, 1: 自瞄, 2: 小符, 3: 大符
  float q[4];    // 云台姿态四元数 wxyz顺序
  float yaw;   // 偏航角
  float yaw_vel; // 偏航角速度
  float pitch;// 俯仰角
  float pitch_vel; // 俯仰角速度
  float bullet_speed;
  uint16_t bullet_count;  // 子弹累计发送次数
  uint8_t crc8;
  uint8_t tail = 0x0d;
};

static_assert(sizeof(GimbalToVision) <= 64);

struct __attribute__((packed)) VisionToGimbal
{
  uint8_t head = 0xff;
  uint8_t mode;  // 0: 不控制, 1: 控制云台但不开火，2: 控制云台且开火
  float yaw;  // 目标偏航角
  float yaw_vel;// 目标偏航角速度
  float yaw_acc; // 目标偏航角加速度
  float pitch;  // 目标俯仰角
  float pitch_vel;  // 目标俯仰角速度
  float pitch_acc; // 目标俯仰角加速度
  uint8_t crc8;
  uint8_t tail = 0x0d;
};

static_assert(sizeof(VisionToGimbal) <= 64);

enum class GimbalMode
{
  IDLE,        // 空闲
  AUTO_AIM,    // 自瞄
  SMALL_BUFF,  // 小符
  BIG_BUFF     // 大符
};

struct GimbalState
{
  float yaw;
  float yaw_vel;
  float pitch;
  float pitch_vel;
  float bullet_speed;
  uint16_t bullet_count;
};

class Gimbal
{
public:
  Gimbal(const std::string & config_path);

  ~Gimbal();

  GimbalMode mode() const;
  GimbalState state() const;
  std::string str(GimbalMode mode) const;
  Eigen::Quaterniond q(std::chrono::steady_clock::time_point t);

  void send(
    bool control, bool fire, float yaw, float yaw_vel, float yaw_acc, float pitch, float pitch_vel,
    float pitch_acc);

  void send(io::VisionToGimbal VisionToGimbal);

private:
  serial::Serial serial_;

  std::thread thread_;
  std::atomic<bool> quit_ = false;
  mutable std::mutex mutex_;

  GimbalToVision rx_data_;
  VisionToGimbal tx_data_;

  GimbalMode mode_ = GimbalMode::IDLE;
  GimbalState state_;
  //新增的
  Eigen::Quaterniond last_q_{1.0, 0.0, 0.0, 0.0};
  bool has_last_q_ = false;

  tools::ThreadSafeQueue<std::tuple<Eigen::Quaterniond, std::chrono::steady_clock::time_point>>
    queue_{1000};

  bool read(uint8_t * buffer, size_t size);
  void read_thread();
  void reconnect();
};

}  // namespace io

#endif  // IO__GIMBAL_HPP