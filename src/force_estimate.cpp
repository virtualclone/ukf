#include <ros/ros.h>
#include "ros/ros.h"
#include "std_msgs/String.h"
#include "sensor_msgs/Imu.h"
#include "forceest.h"
#include "geometry_msgs/PoseStamped.h"
#include <tf/transform_datatypes.h>
#include "geometry_msgs/Point.h"
#include "math.h"
#include "eigen3/Eigen/Core"
#include "eigen3/Eigen/Dense"
#include "geometry_msgs/TwistStamped.h"
#include "geometry_msgs/Point.h"
#include "mavros_msgs/RCOut.h"
#include "sensor_msgs/MagneticField.h"
#include "sensor_msgs/BatteryState.h"
#include "nav_msgs/Odometry.h"
#include <string>
#include <cstdio>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#define l 0.25
#define k 0.02
int drone_flag;
int bias_flag;
forceest forceest1(statesize,measurementsize);
geometry_msgs::Point euler, euler_ref, force, torque, bias, angular_v,pwm,battery_p,pose,thrust, force_nobias, rope_theta_v;
sensor_msgs::Imu drone2_imu;
void imu_cb(const sensor_msgs::Imu::ConstPtr &msg){
  drone2_imu = *msg;
}
sensor_msgs::Imu raw;
void raw_cb(const sensor_msgs::Imu::ConstPtr &msg){
  raw = *msg;
}
geometry_msgs::PoseStamped drone2_pose;
void pose_cb(const geometry_msgs::PoseStamped::ConstPtr &msg){
  drone2_pose = *msg;


}
geometry_msgs::TwistStamped drone2_vel;
void vel_cb(const geometry_msgs::TwistStamped::ConstPtr &msg){
  drone2_vel = *msg;
}

mavros_msgs::RCOut rc_out;
void rc_cb(const mavros_msgs::RCOut::ConstPtr &msg){
  rc_out = *msg;
}
sensor_msgs::MagneticField mag;
void mag_cb(const sensor_msgs::MagneticField::ConstPtr &msg){
  mag = *msg;
}

sensor_msgs::BatteryState battery;
void battery_cb(const sensor_msgs::BatteryState::ConstPtr &msg){
  battery = *msg;
}
nav_msgs::Odometry odom;
void odom_cb(const nav_msgs::Odometry::ConstPtr &msg){
  odom = *msg;
  forceest1.quat_m << drone2_pose.pose.orientation.x, drone2_pose.pose.orientation.y, drone2_pose.pose.orientation.z, drone2_pose.pose.orientation.w;
}
geometry_msgs::Point acc_bias;
void acc_cb(const geometry_msgs::Point::ConstPtr &msg){
  acc_bias = *msg;
}
geometry_msgs::Point gyro_bias;
void gyro_cb(const geometry_msgs::Point::ConstPtr &msg){
  gyro_bias = *msg;
}

char getch()
{
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0) {
        perror("tcsetattr()");
    }
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0) {
        perror("tcsetattr ICANON");
    }
    if (read(0, &buf, 1) < 0) {
        //perror ("read()");
    }
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0) {
        perror ("tcsetattr ~ICANON");
    }
    return (buf);
}
int main(int argc, char **argv)
{
  ros::init(argc, argv, "force_estimate");
  ros::NodeHandle nh;

  std::string topic_imu, topic_mocap, topic_thrust, topic_vel, topic_mag, topic_raw,topic_battery,topic_odom, topic_acc_bias, topic_gyro_bias;
  ros::param::get("~topic_imu", topic_imu);
  ros::param::get("~topic_mocap", topic_mocap);
  ros::param::get("~topic_thrust", topic_thrust);
  ros::param::get("~topic_vel", topic_vel);
  ros::param::get("~topic_vel", topic_vel);
  ros::param::get("~topic_drone", drone_flag);
  ros::param::get("~topic_mag", topic_mag);
  ros::param::get("~topic_raw", topic_raw);
  ros::param::get("~topic_battery", topic_battery);
  ros::param::get("~topic_odom", topic_odom);
  ros::param::get("~topic_acc_bias", topic_acc_bias);
  ros::param::get("~topic_gyro_bias", topic_gyro_bias);
  ros::Subscriber imu_sub = nh.subscribe<sensor_msgs::Imu>(topic_imu, 2, imu_cb);
  ros::Subscriber raw_sub = nh.subscribe<sensor_msgs::Imu>(topic_raw, 2, raw_cb);
  ros::Subscriber pos_sub = nh.subscribe<geometry_msgs::PoseStamped>(topic_mocap, 2, pose_cb);
  ros::Subscriber vel_sub = nh.subscribe<geometry_msgs::TwistStamped>(topic_vel, 2, vel_cb);
  ros::Subscriber rc_sub = nh.subscribe<mavros_msgs::RCOut>(topic_thrust, 2, rc_cb);
  ros::Subscriber mag_sub = nh.subscribe<sensor_msgs::MagneticField>(topic_mag, 2, mag_cb);
  ros::Subscriber battery_sub = nh.subscribe<sensor_msgs::BatteryState>(topic_battery, 2, battery_cb);
  //ros::Subscriber odom_sub = nh.subscribe<nav_msgs::Odometry>(topic_odom, 2, odom_cb);
  ros::Subscriber acc_sub = nh.subscribe<geometry_msgs::Point>(topic_acc_bias, 2, acc_cb);
  ros::Subscriber gyro_sub = nh.subscribe<geometry_msgs::Point>(topic_gyro_bias, 2, gyro_cb);
  ros::Publisher euler_pub = nh.advertise<geometry_msgs::Point>("euler", 2);
  ros::Publisher euler_ref_pub = nh.advertise<geometry_msgs::Point>("euler_ref", 2);
  ros::Publisher force_pub = nh.advertise<geometry_msgs::Point>("force_estimate", 2);
  ros::Publisher torque_pub = nh.advertise<geometry_msgs::Point>("torque", 2);
  ros::Publisher bias_pub = nh.advertise<geometry_msgs::Point>("bias", 2);
  ros::Publisher angular_v_pub = nh.advertise<geometry_msgs::Point>("angular_v", 2);
  ros::Publisher pwm_pub = nh.advertise<geometry_msgs::Point>("pwm", 2);
  ros::Publisher battery_pub = nh.advertise<geometry_msgs::Point>("battery", 2);
  ros::Publisher pose_pub = nh.advertise<geometry_msgs::Point>("pose", 2);
  ros::Publisher thrust_pub = nh.advertise<geometry_msgs::Point>("thrust", 2);
  ros::Publisher force_nobias_pub = nh.advertise<geometry_msgs::Point>("force_nobias", 2);
  ros::Publisher ttt_pub = nh.advertise<geometry_msgs::Point>("ttt", 2);
  ros::Publisher qqq_pub = nh.advertise<geometry_msgs::Point>("qqq", 2);
  ros::Publisher rope_theta_pub = nh.advertise<geometry_msgs::Point>("rope_theta", 2);
  ros::Rate loop_rate(30);


  double measure_ex, measure_ey, measure_ez;
  double sum_pwm;
  int count = 1;
  Eigen::MatrixXd mnoise;
  mnoise.setZero(measurementsize,measurementsize);
  mnoise   = 3e-3*Eigen::MatrixXd::Identity(measurementsize , measurementsize);

  mnoise(mp_x,mp_x) = 1e-4;
  mnoise(mp_y,mp_y) = 1e-4;
  mnoise(mp_z,mp_z) = 1e-4;
  mnoise(mv_x,mv_x) = 1e-2;
  mnoise(mv_y,mv_y) = 1e-2;
  mnoise(mv_z,mv_z) = 1e-2;

  mnoise(momega_x,momega_x) = 1e-2;
  mnoise(momega_y,momega_y) = 1e-2;
  mnoise(momega_z,momega_z) = 1e-2;

  mnoise(me_x,me_x) = 1;//1
  mnoise(me_y,me_y) = 1;
  mnoise(me_z,me_z) = 1;
/*
  mnoise(mq_x,mq_x) = 1e-2;
  mnoise(mq_y,mq_y) = 1e-2;
  mnoise(mq_z,mq_z) = 1e-2;
  mnoise(mq_w,mq_w) = 1e-2;
*/
  forceest1.set_measurement_noise(mnoise);


  Eigen::MatrixXd pnoise;
  pnoise.setZero(statesize,statesize);
  pnoise(p_x,p_x) = 1e-2;
  pnoise(p_y,p_y) = 1e-2;
  pnoise(p_z,p_z) = 1e-2;
  pnoise(v_x,v_x) = 1e-2;
  pnoise(v_y,v_y) = 1e-2;
  pnoise(v_z,v_z) = 1e-2;
  pnoise(e_x,e_x) = 0.005;//0.5,調小beta收斂較快
  pnoise(e_y,e_y) = 0.005;
  pnoise(e_z,e_z) = 0.005;

  pnoise(omega_x,omega_x) = 1e-2;
  pnoise(omega_y,omega_y) = 1e-2;
  pnoise(omega_z,omega_z) = 1e-2;

  pnoise(F_x,F_x) = 0.05;
  pnoise(F_y,F_y) = 0.05;
  pnoise(F_z,F_z) = 0.05;
  pnoise(tau_z,tau_z) = 0.05;

  pnoise(beta_x,beta_x) = 0.05;//調大beta會無法收斂
  pnoise(beta_y,beta_y) = 0.05;
  pnoise(beta_z,beta_z) = 0.05;



  forceest1.set_process_noise(pnoise);



  Eigen::MatrixXd measurement_matrix;
  measurement_matrix.setZero(measurementsize,statesize);

  measurement_matrix(mp_x,p_x) = 1;
  measurement_matrix(mp_y,p_y) = 1;
  measurement_matrix(mp_z,p_z) = 1;


  measurement_matrix(mv_x,v_x) = 1;
  measurement_matrix(mv_y,v_y) = 1;
  measurement_matrix(mv_z,v_z) = 1;


  measurement_matrix(momega_x,omega_x) = 1;
  measurement_matrix(momega_y,omega_y) = 1;
  measurement_matrix(momega_z,omega_z) = 1;

  measurement_matrix(me_x,e_x) = 3.5;//1,調小，beta會劇烈震盪
  measurement_matrix(me_y,e_y) = 3.5;
  measurement_matrix(me_z,e_z) = 3.5;

/*
  measurement_matrix(mq_x,q_x) = 1;
  measurement_matrix(mq_y,q_y) = 1;
  measurement_matrix(mq_z,q_z) = 1;
  measurement_matrix(mq_w,q_w) = 1;
*/



  forceest1.set_measurement_matrix(measurement_matrix);
  int battery_flag2, battery_flag3;
  double start_time,ini_time;
  double m = 1.85;
  ini_time = ros::Time::now().toSec();
  double sum_Fx,sum_Fy,sum_Fz;
  double avg_Fx,avg_Fy,avg_Fz;
  double bias_Fx,bias_Fy,bias_Fz;
  double rope_theta_old;
  int force_count = 1;
  int drone3_battery_flag = 0;
  double drone3_initial_battery;
  double delta_voltage;
  while(ros::ok()){


    int c = getch();
//ROS_INFO("C: %d",c);
    if (c != EOF) {
        switch (c) {
        case 112:    // key p, calculate bias
            bias_flag = 1;
            break;
        case 44: // key < , initial drone3's battery voltage
            drone3_battery_flag = 1;
        }
    }
    double F1, F2, F3, F4;
    double pwm1, pwm2, pwm3, pwm4;
    double U_x, U_y, U_z;
    Eigen::MatrixXd theta_q(4,3), phi_q(4,3);
    Eigen::Matrix3d A_q;
    Eigen::Vector3d y_k;
    Eigen::Vector3d mag_v;
    double battery_dt;
    double rope_theta, rope_omega;
    pose.x = drone2_pose.pose.position.x;
    pose_pub.publish(pose);

    battery_dt = ros::Time::now().toSec() - ini_time;
    if(drone3_battery_flag == 1){
      drone3_initial_battery = battery.voltage;
      drone3_battery_flag == 0;

    }
    ROS_INFO("delta voltage = %f", drone3_initial_battery - battery.voltage);
    if(battery.voltage !=0 && battery.voltage < 10.3 && battery_flag2 == 0){

      battery_flag2 = 1;
    }
    if(battery.voltage !=0 && battery.voltage < 10.6 && battery_flag3 == 0){
      start_time = ros::Time::now().toSec();
      battery_flag3 = 1;
    }

  if(drone2_imu.angular_velocity.x!=0 && drone2_pose.pose.position.x !=0 && drone2_vel.twist.linear.x !=0){
    if(rc_out.channels.size()!=0 && rc_out.channels[0] != 0){

    pwm1 = rc_out.channels[0];
    pwm2 = rc_out.channels[1];
    pwm3 = rc_out.channels[2];
    pwm4 = rc_out.channels[3];

    }

if(drone_flag==3){
  double b;
    F1 = (6.10242*1e-4*(pwm3*pwm3) - 0.66391*pwm3 - 51.04519)*9.8/1000; // drone3
    F2 = (5.912439*1e-4*(pwm1*pwm1) - 0.632553*pwm1 - 75.996)*9.8/1000; //left_right:127.5178
    F3 = (6.146690*1e-4*(pwm4*pwm4) - 0.70247*pwm4 - 13.9731)*9.8/1000; //up_down:97.5178
    F4 = (5.412105*1e-4*(pwm2*pwm2) - 0.493*pwm2 - 165.99)*9.8/1000;
  b = -(3.065625*1000/9.8-5.6590*1e-4*(pwm1*pwm1)+0.5995*pwm1);//no payload
  if(battery_flag3 == 1){
    /*
    pwm1 = pwm1 - (-15*battery.voltage+160);
    pwm2 = pwm2 - (-15*battery.voltage+160);
    pwm3 = pwm3 - (-15*battery.voltage+160);
    pwm4 = pwm4 - (-15*battery.voltage+160);
    F1 = (5.6590*1e-4*(pwm3*pwm3) - 0.5995*pwm3 - 80.5178-25*(1-exp(-1.08*(10.6-battery.voltage))))*9.8/1000; // drone3
    F2 = (5.6590*1e-4*(pwm1*pwm1) - 0.5995*pwm1 - 80.5178-25*(1-exp(-1.08*(10.6-battery.voltage))))*9.8/1000; //left_right:127.5178
    F3 = (5.6590*1e-4*(pwm4*pwm4) - 0.5995*pwm4 - 80.5178-25*(1-exp(-1.08*(10.6-battery.voltage))))*9.8/1000; //up_down:97.5178
    F4 = (5.6590*1e-4*(pwm2*pwm2) - 0.5995*pwm2 - 80.5178-25*(1-exp(-1.08*(10.6-battery.voltage))))*9.8/1000;
    */
    F1 = (6.710368072689550*1e-4*(pwm3*pwm3)-0.89733*pwm3 + 131.7261)*9.8/1000; // drone3
    F2 = (5.482678*1e-4*(pwm1*pwm1) - 0.534949*pwm1 -133.961)*9.8/1000; //left_right:127.5178
    F3 = (5.16589*1e-4*(pwm4*pwm4) - 0.48275*pwm4 - 148.5)*9.8/1000; //up_down:97.5178
    F4 = (6.64161*1e-4*(pwm2*pwm2) - 0.89101*pwm2 + 132.7442)*9.8/1000;
  }
  std::cout << "---b---" << std::endl;
  std::cout << b << std::endl;

}
if(drone_flag==2){
  double a;
  /*
    F1 = (8.1733*1e-4*(pwm3*pwm3) - 1.2950*pwm3 + 265.7775)*9.8/1000; //drone2
    F2 = (8.1733*1e-4*(pwm1*pwm1) - 1.2950*pwm1 + 265.7775)*9.8/1000; //left_right:265.7775
    F3 = (8.1733*1e-4*(pwm4*pwm4) - 1.2950*pwm4 + 265.7775)*9.8/1000; //up_down:265.7775
    F4 = (8.1733*1e-4*(pwm2*pwm2) - 1.2950*pwm2 + 265.7775)*9.8/1000;
    */
  F1 = (8.0274*1e-4*(pwm3*pwm3) - 1.441*pwm3 +  587.9219)*9.8/1000; //drone2
  F2 = (5.167*1e-4*(pwm1*pwm1)  - 0.5049*pwm1 - 106.083)*9.8/1000; //left_right:265.7775
  F3 = (9.74659*1e-4*(pwm4*pwm4) -1.90901*pwm4 + 915.60244)*9.8/1000; //up_down:265.7775
  F4 = (6.04312*1e-4*(pwm2*pwm2) -0.767787*pwm2 + 78.7524)*9.8/1000;

    battery_p.x = F1+F2+F3+F4;
    thrust.x = F2;
    thrust.y = F3;
    thrust.z = F4;
    thrust_pub.publish(thrust);

    if(battery_flag2 == 1){

      F1 = (4.64711*1e-4*(pwm3*pwm3) -0.3953*pwm3 - 177.7)*9.8/1000; //drone2
      F2 = (5.8998*1e-4*(pwm1*pwm1)  -0.7415*pwm1 + 58.4266)*9.8/1000; //left_right:265.7775
      F3 = (5.953219*1e-4*(pwm4*pwm4) -0.83431*pwm4 + 173.47)*9.8/1000; //up_down:265.7775
      F4 = (6.340339*1e-4*(pwm2*pwm2) -0.8930*pwm2 + 173.68)*9.8/1000;


      battery_p.y = F1 + F2 + F3+ F4;
    }

    a = 3.065625*1000/9.8-8.1733*1e-4*(pwm1*pwm1)+1.2950*pwm1;//no payload
    battery_pub.publish(battery_p);
    std::cout << "---a---" << std::endl;
    std::cout << a << std::endl;

}
    forceest1.thrust = F1 + F2 + F3 + F4;
    pwm.x = pwm1+pwm2+pwm3+pwm4;
    sum_pwm =sum_pwm + pwm1+pwm2+pwm3+pwm4;
    pwm.y = sum_pwm / count;
    count = count + 1;
    pwm.z = pwm3;

    pwm_pub.publish(pwm);
    std::cout << "----------thrust-------" << std::endl;
    std::cout << forceest1.thrust << std::endl;

    U_x = (sqrt(2)/2)*l*(F1 - F2 - F3 + F4);
    U_y = (sqrt(2)/2)*l*(-F1 - F2 + F3 + F4);
    U_z = k*F1 - k*F2 + k*F3 - k*F4;
    forceest1.U << U_x, U_y, U_z;
    double x = drone2_pose.pose.orientation.x;
    double y = drone2_pose.pose.orientation.y;
    double z = drone2_pose.pose.orientation.z;
    double w = drone2_pose.pose.orientation.w;

    forceest1.R_IB.setZero();
    forceest1.R_IB << w*w+x*x-y*y-z*z  , 2*x*y-2*w*z ,            2*x*z+2*w*y,
        2*x*y +2*w*z           , w*w-x*x+y*y-z*z    ,2*y*z-2*w*x,
        2*x*z -2*w*y          , 2*y*z+2*w*x        ,w*w-x*x-y*y+z*z;
    forceest1.angular_v_measure << drone2_imu.angular_velocity.x, drone2_imu.angular_velocity.y, drone2_imu.angular_velocity.z;

    forceest1.predict();

    Eigen::VectorXd measure;
    measure.setZero(measurementsize);


    measure << drone2_pose.pose.position.x, drone2_pose.pose.position.y, drone2_pose.pose.position.z,
               drone2_vel.twist.linear.x, drone2_vel.twist.linear.y, drone2_vel.twist.linear.z,
               measure_ex, measure_ey, measure_ez,
               drone2_imu.angular_velocity.x, drone2_imu.angular_velocity.y, drone2_imu.angular_velocity.z;
    forceest1.omega_bias(0) = gyro_bias.x;
    forceest1.omega_bias(1) = gyro_bias.y;
    forceest1.omega_bias(2) = gyro_bias.z;
    forceest1.quat_m << drone2_pose.pose.orientation.x, drone2_pose.pose.orientation.y, drone2_pose.pose.orientation.z, drone2_pose.pose.orientation.w;
    forceest1.qk11 = forceest1.qk1;
/*
    theta_q << forceest1.quaternion(3), -forceest1.quaternion(2), forceest1.quaternion(1),
               forceest1.quaternion(2), forceest1.quaternion(3), -forceest1.quaternion(0),
               -forceest1.quaternion(1), forceest1.quaternion(0), forceest1.quaternion(3),
               -forceest1.quaternion(0), -forceest1.quaternion(1), -forceest1.quaternion(2);
    phi_q << forceest1.quaternion(3), forceest1.quaternion(2), -forceest1.quaternion(1),
               -forceest1.quaternion(2), forceest1.quaternion(3), forceest1.quaternion(0),
               forceest1.quaternion(1), -forceest1.quaternion(0), forceest1.quaternion(3),
               -forceest1.quaternion(0), -forceest1.quaternion(1), -forceest1.quaternion(2);
    A_q = theta_q.transpose()*phi_q;
    mag_v << mag.magnetic_field.x,mag.magnetic_field.y,mag.magnetic_field.z;
    y_k = A_q*mag_v;
    std::cout << "---y_k---" << std::endl;
    std::cout << y_k << std::endl;
*/
    forceest1.correct(measure);
    forceest1.x[e_x] = 0;
    forceest1.x[e_y] = 0;
    forceest1.x[e_z] = 0;
/*
    std::cout << "--------" << std::endl;

    std::cout << "---position---" << std::endl;
    std::cout << forceest1.x[p_x] << std::endl;
    std::cout << forceest1.x[p_y] << std::endl;
    std::cout << forceest1.x[p_z] << std::endl;
    std::cout << "---velocity---" << std::endl;
    std::cout << forceest1.x[v_x] << std::endl;
    std::cout << forceest1.x[v_y] << std::endl;
    std::cout << forceest1.x[v_z] << std::endl;

    std::cout << "---error quaternion---" << std::endl;
    std::cout << forceest1.x[e_x] << std::endl;
    std::cout << forceest1.x[e_y] << std::endl;
    std::cout << forceest1.x[e_z] << std::endl;

    std::cout << "---omega---" << std::endl;
    std::cout << forceest1.x[omega_x] << std::endl;
    std::cout << forceest1.x[omega_y] << std::endl;
    std::cout << forceest1.x[omega_z] << std::endl;
*/

    std::cout << "---Force---" << std::endl;
    std::cout << forceest1.x[F_x] << std::endl;
    std::cout << forceest1.x[F_y] << std::endl;
    std::cout << forceest1.x[F_z] << std::endl;
    std::cout << forceest1.x[tau_z] << std::endl;
/*
    std::cout << "---angular velocity bias---" << std::endl;
    std::cout << forceest1.x[beta_x] << std::endl;
    std::cout << forceest1.x[beta_y] << std::endl;
    std::cout << forceest1.x[beta_z] << std::endl;
*/

    bias.x = forceest1.x[beta_x];
    bias.y = forceest1.x[beta_y];
    bias.z = forceest1.x[beta_z];

    /*
    bias.x = battery.voltage;
    */


    if(bias_flag == 1){

      sum_Fx = sum_Fx + forceest1.x[F_x];
      sum_Fy = sum_Fy + forceest1.x[F_y];
      sum_Fz = sum_Fz + forceest1.x[F_z];
      avg_Fx = sum_Fx/force_count;
      avg_Fy = sum_Fy/force_count;
      avg_Fz = sum_Fz/force_count;
      force_count = force_count + 1;
      if(force_count > 100){
        bias_Fx = avg_Fx;
        bias_Fy = avg_Fy;
        bias_Fz = avg_Fz;
        bias_flag = 0;

      }
    }
    ROS_INFO("Fx = %f, Fy = %f", forceest1.x[F_x], forceest1.x[F_y]);
    ROS_INFO("bias_Fx = %f, bias_Fy = %f", bias_Fx, bias_Fy);
    bias_pub.publish(bias);

    euler.x = forceest1.euler_angle(0);//roll:forceest1.euler_angle(0)
    euler.y = forceest1.euler_angle(1);//pitch:forceest1.euler_angle(1)
    euler.z = forceest1.euler_angle(2);//yaw:forceest1.euler_angle(2)
    euler_pub.publish(euler);

    angular_v.x = drone2_imu.angular_velocity.x;
    angular_v.y = drone2_imu.angular_velocity.y;
    angular_v.z = drone2_imu.angular_velocity.z;

    angular_v_pub.publish(angular_v);
    tf::Quaternion quat_transform_ref(drone2_pose.pose.orientation.x,drone2_pose.pose.orientation.y,drone2_pose.pose.orientation.z,drone2_pose.pose.orientation.w);
    double roll_ref,pitch_ref,yaw_ref;
    tf::Matrix3x3(quat_transform_ref).getRPY(roll_ref,pitch_ref,yaw_ref);
    euler_ref.x = roll_ref*180/3.1415926;//roll_ref*180/3.1415926
    euler_ref.y = pitch_ref*180/3.1415926;//pitch_ref*180/3.1415926
    euler_ref.z = yaw_ref*180/3.1415926;//yaw_ref*180/3.1415926
    euler_ref_pub.publish(euler_ref);

/*
    force.x = forceest1.x[F_x] - m*acc_bias.x;
    force.y = forceest1.x[F_y] - m*acc_bias.y;
    force.z = forceest1.x[F_z] - m*acc_bias.z;
    */
    force.x = forceest1.x[F_x] - bias_Fx;
    force.y = forceest1.x[F_y] - bias_Fy;
    force.z = forceest1.x[F_z] - bias_Fz - 0.23*9.81;
    rope_theta = atan2(force.z,force.y);
    rope_omega = (rope_theta - rope_theta_old)*30;
    rope_theta_old = rope_theta;
    rope_theta_v.x = rope_theta*180/3.1415926;
    rope_theta_v.y = rope_omega;
    ROS_INFO("rope_theta = %f, rope_omega = %f", rope_theta_v.x, rope_theta_v.y);
    rope_theta_pub.publish(rope_theta_v);
    force_nobias.x = forceest1.x[F_x];
    force_nobias.y = forceest1.x[F_y];
    force_nobias.z = forceest1.x[F_z];
    torque.z = forceest1.x[tau_z];
    torque_pub.publish(torque);
    force_pub.publish(force);
    force_nobias_pub.publish(force_nobias);
    ttt_pub.publish(force_nobias);
    qqq_pub.publish(force);
    //ROS_INFO("q_x = %f", forceest1.x[v_x]);
      }
    loop_rate.sleep();
    ros::spinOnce();

   }
}
