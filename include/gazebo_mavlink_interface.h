/*
 * Copyright 2015 Fadri Furrer, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Michael Burri, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Mina Kamel, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Janosch Nikolic, ASL, ETH Zurich, Switzerland
 * Copyright 2015 Markus Achtelik, ASL, ETH Zurich, Switzerland
 * Copyright 2015-2018 PX4 Pro Development Team
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MAVLINK_INTERFACE_HH_
#define MAVLINK_INTERFACE_HH_

#include <vector>
#include <regex>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_array.hpp>
#include <boost/system/system_error.hpp>

#include <iostream>
#include <random>
#include <stdio.h>
#include <math.h>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

#include <gz/common5/gz/common.hh>
#include <gz/sim/System.hh>
#include <gz/sim/Events.hh>
#include <gz/sim/EventManager.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/Util.hh>
#include "gz/sim/components/Actuators.hh"
#include <gz/sim/components/AngularVelocity.hh>
#include <gz/sim/components/Imu.hh>
#include <gz/sim/components/JointForceCmd.hh>
#include <gz/sim/components/JointPosition.hh>
#include <gz/sim/components/JointVelocity.hh>
#include <gz/sim/components/JointVelocityCmd.hh>
#include <gz/sim/components/LinearVelocity.hh>
#include <gz/sim/components/Name.hh>
#include <gz/sim/components/Pose.hh>

#include <gz/transport/Node.hh>
#include <gz/msgs.hh>
#include <gz/math.hh>

#include <common.h>

#include "mavlink_interface.h"
#include "msgbuffer.h"


using lock_guard = std::lock_guard<std::recursive_mutex>;

//! Default distance sensor model joint naming
static const std::regex kDefaultLidarModelLinkNaming("(lidar|sf10a)(.*::link)");
static const std::regex kDefaultSonarModelLinkNaming("(sonar|mb1240-xl-ez4)(.*::link)");

// Default values
static const std::string kDefaultNamespace = "";

// This just proxies the motor commands from command/motor_speed to the single motors via internal
// ConsPtr passing, such that the original commands don't have to go n_motors-times over the wire.
static const std::string kDefaultMotorVelocityReferencePubTopic = "/gazebo/command/motor_speed";

static const std::string kDefaultPoseTopic = "/pose";
static const std::string kDefaultImuTopic = "/imu";
static const std::string kDefaultOpticalFlowTopic = "/px4flow/link/opticalFlow";
static const std::string kDefaultIRLockTopic = "/camera/link/irlock";
static const std::string kDefaultGPSTopic = "/gps";
static const std::string kDefaultVisionTopic = "/vision_odom";
static const std::string kDefaultMagTopic = "/magnetometer";
static const std::string kDefaultBarometerTopic = "/air_pressure";
static const std::string kDefaultCmdVelTopic = "/cmd_vel";

namespace mavlink_interface
{
  class GZ_SIM_VISIBLE GazeboMavlinkInterface:
    public gz::sim::System,
    public gz::sim::ISystemConfigure,
    public gz::sim::ISystemPreUpdate,
    public gz::sim::ISystemPostUpdate
  {
    public: GazeboMavlinkInterface();

    public: ~GazeboMavlinkInterface() override;
    public: void Configure(const gz::sim::Entity &_entity,
                            const std::shared_ptr<const sdf::Element> &_sdf,
                            gz::sim::EntityComponentManager &_ecm,
                            gz::sim::EventManager &/*_eventMgr*/);
    public: void PreUpdate(const gz::sim::UpdateInfo &_info,
                gz::sim::EntityComponentManager &_ecm);
    public: void PostUpdate(const gz::sim::UpdateInfo &_info,
                const gz::sim::EntityComponentManager &_ecm) override;
    private:
      gz::common::ConnectionPtr sigIntConnection_;
      std::shared_ptr<MavlinkInterface> mavlink_interface_;
      bool received_first_actuator_{false};
      Eigen::VectorXd motor_input_reference_;
      Eigen::VectorXd servo_input_reference_;
      float cmd_vel_thrust_{0.0};
      float cmd_vel_torque_{0.0};

      gz::sim::Entity entity_{gz::sim::kNullEntity};
      gz::sim::Model model_{gz::sim::kNullEntity};
      gz::sim::Entity modelLink_{gz::sim::kNullEntity};
      std::string model_name_;

      float protocol_version_{2.0};

      std::string namespace_{kDefaultNamespace};
      std::string mavlink_control_sub_topic_;
      std::string link_name_;

      bool use_propeller_pid_{false};
      bool use_elevator_pid_{false};
      bool use_left_elevon_pid_{false};
      bool use_right_elevon_pid_{false};

      void PoseCallback(const gz::msgs::Pose_V &_msg);
      void ImuCallback(const gz::msgs::IMU &_msg);
      void BarometerCallback(const gz::msgs::FluidPressure &_msg);
      void MagnetometerCallback(const gz::msgs::Magnetometer &_msg);
      void GpsCallback(const gz::msgs::NavSat &_msg);
      void SendSensorMessages(const gz::sim::UpdateInfo &_info);
      void PublishMotorVelocities(gz::sim::EntityComponentManager &_ecm,
          const Eigen::VectorXd &_vels);
      void PublishServoVelocities(const Eigen::VectorXd &_vels);
      void PublishCmdVelocities(const float _thrust, const float _torque);
      void handle_actuator_controls(const gz::sim::UpdateInfo &_info);
      void onSigInt();
      bool IsRunning();
      bool resolveHostName();
      void ResolveWorker();
      float AddSimpleNoise(float value, float mean, float stddev);
      void RotateQuaternion(gz::math::Quaterniond &q_FRD_to_NED,
        const gz::math::Quaterniond q_FLU_to_ENU);
      void ParseMulticopterMotorModelPlugins(const std::string &sdfFilePath);

      static const unsigned n_out_max = 16;
      static const unsigned n_motors = 4;

      double input_offset_[n_out_max];
      std::string joint_control_type_[n_out_max];
      std::string gztopic_[n_out_max];
      double zero_position_disarmed_[n_out_max];
      double zero_position_armed_[n_out_max];
      int motor_input_index_[n_out_max];
      double motor_vel_scalings_[n_out_max] {1.0};
      int servo_input_index_[n_out_max];
      bool input_is_cmd_vel_{false};

      /// \brief gz communication node and publishers.
      gz::transport::Node node;
      gz::transport::Node::Publisher servo_control_pub_[n_out_max];
      gz::transport::Node::Publisher motor_velocity_pub_;
      gz::transport::Node::Publisher cmd_vel_pub_;

      std::string pose_sub_topic_{kDefaultPoseTopic};
      std::string imu_sub_topic_{kDefaultImuTopic};
      std::string opticalFlow_sub_topic_{kDefaultOpticalFlowTopic};
      std::string irlock_sub_topic_{kDefaultIRLockTopic};
      std::string gps_sub_topic_{kDefaultGPSTopic};
      std::string vision_sub_topic_{kDefaultVisionTopic};
      std::string mag_sub_topic_{kDefaultMagTopic};
      std::string baro_sub_topic_{kDefaultBarometerTopic};
      std::string cmd_vel_sub_topic_{kDefaultCmdVelTopic};

      std::mutex last_imu_message_mutex_ {};

      gz::msgs::IMU last_imu_message_;
      gz::msgs::Actuators motor_velocity_message_;

      std::chrono::steady_clock::duration last_imu_time_{0};
      std::chrono::steady_clock::duration lastControllerUpdateTime{0};
      std::chrono::steady_clock::duration last_actuator_time_{0};

      bool mag_updated_{false};
      bool baro_updated_;
      bool diff_press_updated_;

      double imu_update_interval_ = 0.004; ///< Used for non-lockstep

      gz::math::Vector3d gravity_W_{gz::math::Vector3d(0.0, 0.0, -9.8)};
      gz::math::Vector3d velocity_prev_W_;
      gz::math::Vector3d mag_n_;

      double temperature_;
      double pressure_alt_;
      double abs_pressure_;

      bool close_conn_ = false;

      double optflow_distance;
      double sonar_distance;

      bool enable_lockstep_ = false;
      double speed_factor_ = 1.0;
      uint8_t previous_imu_seq_ = 0;
      uint8_t update_skip_factor_ = 1;

      std::string mavlink_hostname_str_;
      struct hostent *hostptr_{nullptr};
      bool mavlink_loaded_{false};
      std::thread hostname_resolver_thread_;

      std::atomic<bool> gotSigInt_ {false};

      std::default_random_engine rnd_gen_;
  };
}

#endif
