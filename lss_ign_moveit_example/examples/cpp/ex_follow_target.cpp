/// Example that uses MoveIt 2 to follow a target inside Ignition Gazebo
/// Because the Arms only have 4/5 DOF the goal orientation is adjusted so it
/// is always parallel to the base of the robot

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <moveit/move_group_interface/move_group_interface.h>
#include <rclcpp/qos.hpp>
#include <rclcpp/rclcpp.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

const std::string MOVE_GROUP = "lss_arm";

class MoveItFollowTarget : public rclcpp::Node
{
public:
  /// Constructor
  MoveItFollowTarget();

  /// Move group interface for the robot
  moveit::planning_interface::MoveGroupInterface move_group_;
  /// Subscriber for target pose
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr target_pose_sub_;
  /// Target pose that is used to detect changes
  geometry_msgs::msg::Pose previous_target_pose_;

private:
  /// Callback that plans and executes trajectory each time the target pose is changed
  void target_pose_callback(const geometry_msgs::msg::PoseStamped::ConstSharedPtr msg);
};

MoveItFollowTarget::MoveItFollowTarget() : Node("ex_follow_target"),
                                           move_group_(std::shared_ptr<rclcpp::Node>(std::move(this)), MOVE_GROUP)
{
  // Use upper joint velocity and acceleration limits
  this->move_group_.setMaxAccelerationScalingFactor(1.0);
  this->move_group_.setMaxVelocityScalingFactor(1.0);

  // Subscribe to target pose
  target_pose_sub_ = this->create_subscription<geometry_msgs::msg::PoseStamped>("/target_pose", rclcpp::QoS(1), std::bind(&MoveItFollowTarget::target_pose_callback, this, std::placeholders::_1));

  RCLCPP_INFO(this->get_logger(), "Initialization successful.");
}

void MoveItFollowTarget::target_pose_callback(const geometry_msgs::msg::PoseStamped::ConstSharedPtr msg)
{
  // Return if target pose is unchanged
  if (msg->pose == previous_target_pose_)
  {
    return;
  }

  RCLCPP_INFO(this->get_logger(), "Target pose has changed. Planning and executing...");

  // Plan and execute motion towards target pose
  this->move_group_.setPoseTarget(msg->pose);
  moveit::planning_interface::MoveItErrorCode success = this->move_group_.move();

  // If unable to move to target pose, try pose_position_only.pose
  if (success != moveit::planning_interface::MoveItErrorCode::SUCCESS) {
    // Calculate the yaw angle based on the target position and the base
    double yaw = atan2(msg->pose.position.y, msg->pose.position.x);

    // Create a quaternion from the Euler angles
    tf2::Quaternion q;
    q.setRPY(1.57, 0.0, yaw); // roll, pitch, yaw

    // Copy the desired pose (goal position w/o orientation)
    geometry_msgs::msg::PoseStamped pose_position_only = *msg;

    // Set default orientation (always parallel to the base)
    pose_position_only.pose.orientation = tf2::toMsg(q);
  
    this->move_group_.setPoseTarget(pose_position_only.pose);
    this->move_group_.move();
  }

  // Update for next callback
  previous_target_pose_ = msg->pose;
}

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);

  auto target_follower = std::make_shared<MoveItFollowTarget>();

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(target_follower);
  executor.spin();

  rclcpp::shutdown();
  return EXIT_SUCCESS;
}
