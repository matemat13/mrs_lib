#ifndef PROFILER_H
#define PROFILER_H

#include <ros/ros.h>
#include <mrs_msgs/ProfilerUpdate.h>
#include <mutex>

namespace mrs_lib
{

/* class Routine //{ */

class Routine {

public:
  Routine(std::string name, std::string node_name, std::shared_ptr<ros::Publisher> publisher, std::shared_ptr<std::mutex> mutex_publisher,
          bool profiler_enabled);
  Routine(std::string name, std::string node_name, int expected_rate, double threshold, std::shared_ptr<ros::Publisher> publisher,
          std::shared_ptr<std::mutex> mutex_publisher, bool profiler_enabled, ros::TimerEvent event);
  ~Routine();

  void end(void);

private:
  std::string routine_name;
  std::string node_name;

  std::shared_ptr<ros::Publisher> publisher;
  std::shared_ptr<std::mutex>     mutex_publisher;

  // if periodic, those are the stats from the trigger event
  double threshold_;
  long   iteration = 0;

  bool profiler_enabled_ = false;

  // those are the stats from the execution of the routine
  ros::Time execution_start;

  // this will be published
  mrs_msgs::ProfilerUpdate msg_out;
};

//}

/* class Profiler //{ */

class Profiler {

public:
  Profiler(ros::NodeHandle &nh_, std::string node_name, bool profiler_enabled);

  Routine createRoutine(std::string name, int expected_rate, double threshold, ros::TimerEvent event);
  Routine createRoutine(std::string name);

private:
  std::shared_ptr<ros::Publisher> publisher;
  std::shared_ptr<std::mutex>     mutex_publisher;
  std::string                     node_name;
  bool                            profiler_enabled_ = false;
};

//}
}  // namespace mrs_lib

#endif
