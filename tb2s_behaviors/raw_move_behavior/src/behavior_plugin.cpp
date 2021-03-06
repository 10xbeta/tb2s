// Copyright 2018 Matt Curfman, Dave Shinsel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This behavior will do a "Raw" move, meaning, it does not use sensors to determine path or safety!  It just moves!

#include <ros/ros.h>
#include <pluginlib/class_list_macros.h>
#include <behavior_common/behavior.h>
#include <behavior_common/behavior_common.h>
#include <nav_msgs/Odometry.h>
//#include <tf/transform_datatypes.h>
//#include <tf/LinearMath/Matrix3x3.h>
//#include <angles/angles.h>
#include <string>


namespace behavior_plugin 
{

  enum State
  {
  	idle,
  	initializing_start_position,
  	moving,
    ending
  };
	
  class RawMoveService : public behavior_common::BehaviorActionServiceBase
  {
  public:
    RawMoveService(std::string service_name) :
      behavior_common::BehaviorActionServiceBase(service_name),
      state_(idle),
      DEFAULT_SPEED(0.3), // meters per second
      speed_(DEFAULT_SPEED),
      goal_move_amount_(0.0),
      start_position_x_(0.0),
      start_position_y_(0.0),
      ramp_down_fudge_(0.09)  // Compensate for velocity smoother ramp down
    {
      // Init publishers and subscribers
      // rotation is monitored in odometryCallback
      cmd_vel_pub_ = nh_.advertise<geometry_msgs::Twist>("/cmd_vel_mux/realsense", 1); 
    }

    virtual ~RawMoveService()
    {
      // Stop moving before exiting
      state_ = idle;    
      stop();
    }

    virtual void StartBehavior(const char *param1, const char *param2)
    {
      // Param1: amount in meters ()
      // Param2: speed
      ROS_INFO("MOVE Param1 = %s, Param2= %s", param1, param2);

      goal_move_amount_ = atof(param1); // negative = backup
      speed_ = fabs(atof(param2));      // speed input is always positive
      if(0 == speed_)
      {
       speed_ = DEFAULT_SPEED;
      }
      if(goal_move_amount_ < 0.0)
      {
        speed_ *= -1.0;
        goal_move_amount_ *= -1.0;
      }
      

      ROS_INFO("MOVE: Goal move amount: %f", goal_move_amount_);
      ROS_INFO("MOVE: Move speed: %f", speed_);
      odometrySubscriber_ = nh_.subscribe("/odom", 1, &RawMoveService::odometryCallback, this);
      state_ = initializing_start_position;
    }

    virtual void PremptBehavior()
    {
      // TODO: Add code which prempts your running behavior here.
      state_ = idle;
      stop();
      odometrySubscriber_.shutdown();
    }

    void odometryCallback(const nav_msgs::Odometry::ConstPtr& msg)
    {
      // Called on each odometry update (~50hz)
 
      #if 0
      ROS_INFO("Pose Position: x: [%f], y: [%f], z: [%f]", 
	      msg->pose.pose.position.x, msg->pose.pose.position.y, msg->pose.pose.position.z);
      #endif

      if(initializing_start_position == state_)
      {
        start_position_x_ = msg->pose.pose.position.x;
        start_position_y_ = msg->pose.pose.position.y;
        ROS_INFO("MOVE: Start Position:  x = %f, y = %f", start_position_x_, start_position_y_);

        // Start moving
        geometry_msgs::Twist cmd;
        cmd.linear.x = speed_;
        cmd.angular.z = 0;
        cmd_vel_pub_.publish(cmd);
        state_ = moving;

      }
      else if(moving == state_)
      {
        // Monitor movement, stop when within goal position
        double current_position_x = msg->pose.pose.position.x;
        double current_position_y = msg->pose.pose.position.y;
        double distance_moved = sqrt(pow((current_position_x - start_position_x_), 2) +
                                     pow((current_position_y - start_position_y_), 2));

        double distance_remaining = fabs(goal_move_amount_) - (fabs(distance_moved) + ramp_down_fudge_);

        if( fabs(distance_moved) > 0.01) // display once move actually starts
        {
  	  	  ROS_INFO("Distance Moved: %f,  Remaining: %f ", distance_moved, distance_remaining);
        }

        if(distance_remaining < 0.0)
        {
		      ROS_INFO("Move Complete!  Stopping.");
          stop();
    	    // Mark behavior complete
          BehaviorComplete();  
          state_ = ending;
        }
        else
        {
          // continue to move
          geometry_msgs::Twist cmd;
          cmd.linear.x = speed_;
          cmd.angular.z = 0;
          cmd_vel_pub_.publish(cmd);

        }
      }
      else if (ending == state_)
      {
        double current_position_x = msg->pose.pose.position.x;
        double current_position_y = msg->pose.pose.position.y;
        double distance_moved = sqrt(pow((current_position_x - start_position_x_), 2) +
                                     pow((current_position_y - start_position_y_), 2));
        double distance_remaining = fabs(goal_move_amount_) - (fabs(distance_moved) + ramp_down_fudge_);
        ROS_INFO("DBG: Final Distance Moved: %f,  Remaining: %f ", distance_moved, distance_remaining);
        state_ = idle;
      }
    }


    // Utility Functions

    void stop()
    {
      geometry_msgs::Twist cmd;
      cmd.linear.x = cmd.angular.z = 0;  
      cmd_vel_pub_.publish(cmd); 
    }


  protected:
    ros::Subscriber odometrySubscriber_;
    ros::NodeHandle nh_;
    ros::Publisher cmd_vel_pub_;

    State  state_;
    const double DEFAULT_SPEED;
    double speed_;
    double goal_move_amount_;
    double start_position_x_;
    double start_position_y_;
    double ramp_down_fudge_;


  };

    CPP_BEHAVIOR_PLUGIN(RawMoveBehaviorPlugin, "/raw_move_service", "RAW_MOVE", RawMoveService);
  };

PLUGINLIB_EXPORT_CLASS(behavior_plugin::RawMoveBehaviorPlugin, behavior_common::BehaviorPlugin);
