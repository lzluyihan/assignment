#ifndef IO__COMMAND_HPP
#define IO__COMMAND_HPP

namespace io {
struct Command {
  bool control;                // 是否有控制权
  bool shoot;                  // 开火建议
  double yaw;                  // 云台自瞄yaw目标值
  double pitch;                // 云台自瞄pitch目标值
  double horizon_distance = 0; // 无人机专有
};

} // namespace io

#endif // IO__COMMAND_HPP
