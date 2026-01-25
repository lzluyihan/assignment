#ifndef IO__Dm_Imu_HPP
#define IO__Dm_Imu_HPP

#include <math.h>
#include <serial/serial.h>

#include <Eigen/Geometry>
#include <array>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <thread>

#include "tools/thread_safe_queue.hpp"

namespace io
{

struct __attribute__((packed)) IMU_Receive_Frame
{
  uint8_t FrameHeader1;    // 第一帧头（1字节）
  uint8_t flag1;           // 第一帧标志位（1字节）
  uint8_t slave_id1;       // 第一帧从机ID（1字节）
  uint8_t reg_acc;         // 加速度计寄存器地址（1字节）
  uint32_t accx_u32;       // X轴加速度（4字节，二进制转float用）
  uint32_t accy_u32;       // Y轴加速度（4字节）
  uint32_t accz_u32;       // Z轴加速度（4字节）
  uint16_t crc1;           // 第一帧CRC16校验值（2字节）
  uint8_t FrameEnd1;       // 第一帧结束位（1字节）

  uint8_t FrameHeader2;    // 第二帧头（陀螺仪数据帧）
  uint8_t flag2;
  uint8_t slave_id2;
  uint8_t reg_gyro;        // 陀螺仪寄存器地址
  uint32_t gyrox_u32;      // X轴角速度
  uint32_t gyroy_u32;      // Y轴角速度
  uint32_t gyroz_u32;      // Z轴角速度
  uint16_t crc2;           // 第二帧CRC校验值
  uint8_t FrameEnd2;

  uint8_t FrameHeader3;    // 第三帧头（欧拉角数据帧）
  uint8_t flag3;
  uint8_t slave_id3;
  uint8_t reg_euler;       // 欧拉角寄存器地址（roll/pitch/yaw）
  uint32_t roll_u32;       // 横滚角
  uint32_t pitch_u32;      // 俯仰角
  uint32_t yaw_u32;        // 偏航角
  uint16_t crc3;           // 第三帧CRC校验值
  uint8_t FrameEnd3;
};

typedef struct
{
  float accx;
  float accy;
  float accz;
  float gyrox;
  float gyroy;
  float gyroz;
  float roll;
  float pitch;
  float yaw;
} IMU_Data;

class DM_IMU
{
public:
  DM_IMU();
  ~DM_IMU();

  Eigen::Quaterniond imu_at(std::chrono::steady_clock::time_point timestamp);

private:
  struct IMUData
  {
    Eigen::Quaterniond q;
    std::chrono::steady_clock::time_point timestamp;
  };

  void init_serial();
  void get_imu_data_thread();

  serial::Serial serial_;
  std::thread rec_thread_;

  tools::ThreadSafeQueue<IMUData> queue_;
  IMUData data_ahead_, data_behind_;

  std::atomic<bool> stop_thread_{false};
  IMU_Receive_Frame receive_data{};  //receive data frame
  IMU_Data data{};
};

}  // namespace io

#endif
