/******************************************************************************
 * Copyright 2023 The XJU AutoPilot Authors. All Rights Reserved.
 *****************************************************************************/
#include "planning/tasks/deciders/speed_decider/speed_decider.h"
#include "planning/tasks/deciders/st_graph_decider/st_graph_decider.h"

#include <cmath>
#include <string>

#include "gtest/gtest.h"
#include "chassis.pb.h"
#include "prediction_obstacle.pb.h"
#include "pad.pb.h"
#include "planning_config.pb.h"
#include "planning_task_config.pb.h"
#include "common/file/file.h"
#include "common/time/time.h"
#include "common/vehicle_state/vehicle_state_provider.h"
#include "common/logger/logger.h"
#include "planning/common/frame/frame.h"
#include "planning/common/local_view.h"
#include "planning/common/planning_internal/planning_internal.h"

namespace xju {
namespace planning {

class SpeedDeciderTest : public testing::Test {
 public:
  virtual void SetUp() {
    LoadPlanningConfig();
    LoadTaskConfig();
    LoadInput();
    double current_timestamp = 1173545122.22;
    bool status = pnc::VehicleStateProvider::Update(
        *(local_view_.localization), *(local_view_.chassis), 0.0, true);
    pnc::VehicleState vehicle_state = pnc::VehicleStateProvider::GetVehicleState();
    auto reference_lines = GenerateReferenceLines();
    pnc::TrajectoryPoint planning_start_point = PlanningStartPoint();
    frame_.Init(current_timestamp, planning_start_point, vehicle_state, reference_lines, local_view_);
    reference_line_info_ = frame_.GetReferenceLineInfos()->front();
    UpdateReferenceLineInfo(&reference_line_info_);
    internal_ = std::make_shared<PlanningInternal>();
  }


  void LoadPlanningConfig() {
    std::string planning_config_file = "/home/ws/src/pnc/configs/planning/planning.pb.txt";
    std::string gflag_config_file_path = "/home/ws/src/pnc/configs/planning/planning.conf";
    pnc::File::GetGflagConfig(gflag_config_file_path);
    pnc::File::GetProtoConfig(planning_config_file, &config_);
    pnc::VehicleStateProvider::Init(config_.vehicle_state_config());
  }

  void LoadTaskConfig() {
    for (const auto& task_config : config_.default_task_config()) {
      if (task_config.task_type() == pnc::TaskType::ST_GRAPH_DECIDER) {
        st_graph_task_config_ = task_config;
      } else if (task_config.task_type() == pnc::TaskType::SPEED_DECIDER) {
        speed_task_config_ = task_config;
      }
    }
    return;
  }

  void LoadInput() {
    std::string prediction_file = "/home/ws/src/pnc/test_data/prediction_obstacles.pb.txt";
    std::string chassis_file = "/home/ws/src/pnc/test_data/chassis.pb.txt";
    std::string localization_file = "/home/ws/src/pnc/test_data/localization.pb.txt";
    std::string pad_file = "/home/ws/src/pnc/test_data/pad_message.pb.txt";
    pnc::File::GetProtoConfig(prediction_file, &prediction_obstacles_);
    pnc::File::GetProtoConfig(chassis_file, &chassis_);
    pnc::File::GetProtoConfig(localization_file, &localization_);
    pnc::File::GetProtoConfig(pad_file, &pad_message_);

    local_view_.prediction_obstacles = std::make_shared<pnc::PredictionObstacles>(prediction_obstacles_);
    local_view_.chassis = std::make_shared<pnc::Chassis>(chassis_);
    local_view_.localization = std::make_shared<pnc::Localization>(localization_);
    local_view_.pad_msg = std::make_shared<pnc::PadMessage>(pad_message_);
    
    AINFO << "mass = " << chassis_.mass();
    AINFO << "obstacle num is " << prediction_obstacles_.prediction_obstacle_size();
  }

  std::list<pnc_map::ReferenceLine> GenerateReferenceLines() {
    std::list<pnc_map::ReferenceLine> ref_lines;
    for (int k = 0; k < 3; ++k) {
      pnc_map::ReferenceLine ref_line;
      std::vector<pnc::PathPoint> points;
      double x = 0.0;
      double y = 0.0;
      if (k == 1) {
        x = -3.75;
      } else if (k == 2) {
        x = 3.75;
      }
      for (int i = 0; i < 200; ++i) {
        pnc::PathPoint pt;
        pt.set_x(x);
        pt.set_y(i);
        pt.set_z(0.0);
        pt.set_theta(M_PI_2);
        pt.set_kappa(0.0);
        pt.set_dkappa(0.0);
        pt.set_ddkappa(0.0);
        pt.set_s(i);
        points.push_back(pt);
      }

      ref_line.set_path_points(points);
      ref_line.set_lane_width(3.75 / 2.0);
      ref_line.set_min_speed_limit(0.0);
      ref_line.set_max_speed_limit(120.0 / 3.6);
      ref_line.set_left_lane_marking_type(pnc::LaneMarkingType::SINGLE_WHITE_DASHED_LINE);
      ref_line.set_right_lane_marking_type(pnc::LaneMarkingType::SINGLE_WHITE_DASHED_LINE);
      ref_line.set_accommodation_lane_type(pnc::AccommodationLaneType::NONE_ACCOMMODATION_LANE_TYPE);
      ref_line.set_is_emergency_lane(false);
      ref_line.set_is_death_lane(false);
      ref_line.set_is_recommended_lane(false);
      ref_line.AddSpeedLimit(0.0, 100, 120 / 3.6);
      ref_lines.push_back(ref_line);
    }

    auto ref_line = ref_lines.begin();
    ref_line->set_id(std::to_string(0));
    ref_line->set_left_lane_id(std::to_string(1));
    ref_line->set_right_lane_id(std::to_string(-1));
    ref_line->set_lane_order(pnc::LaneOrder::SECOND_LANE);

    ++ref_line;
    ref_line->set_id(std::to_string(1));
    ref_line->set_left_lane_id("");
    ref_line->set_right_lane_id(std::to_string(0));
    ref_line->set_lane_order(pnc::LaneOrder::THIRD_LANE);
    ref_line->set_left_lane_marking_type(pnc::LaneMarkingType::SINGLE_WHITE_SOLID_LINE);

    ++ref_line;
    ref_line->set_id(std::to_string(-1));
    ref_line->set_left_lane_id(std::to_string(0));
    ref_line->set_right_lane_id("");
    ref_line->set_lane_order(pnc::LaneOrder::FIRST_LANE);
    ref_line->set_right_lane_marking_type(pnc::LaneMarkingType::SINGLE_WHITE_SOLID_LINE);

    return ref_lines;
  }
  
  pnc::TrajectoryPoint PlanningStartPoint() {
    pnc::TrajectoryPoint tp;
    auto path_point = tp.mutable_path_point();
    path_point->set_x(0.0);
    path_point->set_y(10.0);
    path_point->set_z(0.0);
    path_point->set_theta(M_PI_2);
    path_point->set_kappa(0.0);
    path_point->set_dkappa(0.0);
    path_point->set_ddkappa(0.0);
    path_point->set_s(0.0);

    tp.set_v(0.0);
    tp.set_a(0.0);
    tp.set_da(0.0);
    tp.set_relative_time(0.0);
    return tp;
  }

  PathData GeneratePathData() {
    PathData path_data;
    std::vector<pnc::PathPoint> path_points;
    for (int i = 0; i < 150; ++i) {
        pnc::PathPoint pt;
        pt.set_x(0);
        pt.set_y(i + 10);
        pt.set_z(0.0);
        pt.set_theta(M_PI_2);
        pt.set_kappa(0.0);
        pt.set_dkappa(0.0);
        pt.set_ddkappa(0.0);
        pt.set_s(i / 2.0);
        path_points.push_back(pt);     
    }
    path_data.set_reference_points(path_points);

    std::vector<pnc::FrenetFramePoint> frenet_points;
    for (int i = 0; i < 150; ++i) {
        pnc::FrenetFramePoint pt;
        pt.set_s(i);
        pt.set_l(0);
        pt.set_heading_error(0);
        pt.set_kappa(0);
        frenet_points.push_back(pt);
    }
    path_data.set_frenet_points(frenet_points);
    return path_data;
  }

  void UpdateReferenceLineInfo(std::shared_ptr<ReferenceLineInfo> reference_line_info) {  
    auto path_data = GeneratePathData();
    PathData* mutable_path_data = reference_line_info->mutable_path_data();
    *mutable_path_data = path_data;
    reference_line_info->set_cruise_speed(33.0);
    // PathDecision* path_decision = reference_line_info->path_decision();
    // CHECK_NOTNULL(path_decision);
    // for (auto* obstacle_ptr : path_decision->obstacles().Items()) {
    //   CHECK_NOTNULL(obstacle_ptr);
    //   if (obstacle_ptr->is_static()) {
    //       ObjectDecisionType stop_decision;
    //       stop_decision.mutable_stop();
    //       path_decision->SetLongitudinalDecision(obstacle_ptr->id(), stop_decision);
    //   }
    // }
  }
 protected:
    planning::PlanningConfig config_;
    pnc::TaskConfig st_graph_task_config_;
    pnc::TaskConfig speed_task_config_;
    pnc::PredictionObstacles prediction_obstacles_;
    pnc::Chassis chassis_;
    pnc::Localization localization_;
    pnc::PadMessage pad_message_; 

    planning::Frame frame_;
    planning::LocalView local_view_;
    planning::ReferenceLineInfo reference_line_info_;
    std::shared_ptr<PlanningInternal> internal_;
};

TEST_F(SpeedDeciderTest, Process) {
  StGraphDecider st_graph_decider(st_graph_task_config_, internal_);
  bool ret_1 = st_graph_decider.Process(&reference_line_info_, &frame_);
  
  SpeedDecider speed_decider(speed_task_config_, internal_);
  bool ret = speed_decider.Process(&reference_line_info_, &frame_);
  PathDecision* path_decision = reference_line_info_.path_decision();
  for (const auto* obstacle : path_decision->obstacles().Items()) {
    if (obstacle->HasLongitudinalDecision() &&
        obstacle->longitudinal_decision().has_follow()) {
        ADEBUG << "obstacle: " << obstacle->id()
               << " has follow longitudinal_decision";
    }
  }

  // EXPECT_TRUE(ret);

}
} // namespace planning
} // namespace xju
