/*
RRST NHK2025
汎用機の機構制御

クリコア　(ロンリウム)
=== Current Parameters ===
0: roller_speed_dribble_ab = 15
1: roller_speed_dribble_cd = 75
2: roller_speed_shoot_ab   = 35
3: roller_speed_shoot_cd   = 35
Shoot State: 0
Dribble State: 0
==========================
*/

// 標準
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

// ROS
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32_multi_array.hpp>

// 自作クラス
#include "include/UDP.hpp"

// 各ローラーの速度を指定(%)
int roller_speed_dribble_ab = 30;
int roller_speed_dribble_cd = 30;
int roller_speed_shoot_ab = 50;
int roller_speed_shoot_cd = 50;
int roller_speed_reload = 15;

// IPアドレスとポートの指定
std::string udp_ip =
    "192.168.8.216"; // 送信先IPアドレス、宛先マイコンで設定したIPv4アドレスを指定
int udp_port = 5000; // 送信元ポート番号、宛先マイコンで設定したポート番号を指定

std::vector<int> data = {0, 0,  0,  0, 0,
                         0, -1, -1, -1}; // 7~9番を電磁弁制御に転用中（-1 or 1）

bool stop_requested = false;

// 各機構のシーケンスを格納するクラス
class Action {
public:
  // 事故防止のため、射出機構の展開状況を保存
  static bool ready_for_shoot;
  // 射出機構展開シーケンス
  // 射出機構展開シーケンス

  // 射出シーケンス
  static void shoot_action(UDP &udp) {
    std::cout << "シュート" << std::endl;
    data[7] = 1;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "格納準備中..." << std::endl;
    data[7] = -1;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "格納中..." << std::endl;
    data[6] = -1;
    data[8] = -1;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    udp.send(data);
    ready_for_shoot = false;
    std::cout << "完了." << std::endl;
    std::cout << "<射出シーケンス終了>" << std::endl;
  }

  // ドリブルシーケンス
  static void dribble_action(UDP &udp) {
    std::cout << "<ドリブルシーケンス開始>" << std::endl;
    std::cout << "ドリブル準備中" << std::endl;
    data[1] = roller_speed_dribble_ab;
    data[2] = roller_speed_dribble_ab;
    data[3] = -roller_speed_dribble_cd;
    data[4] = -roller_speed_dribble_cd;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "ドリブル" << std::endl;
    data[8] = 1;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    data[8] = -1;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    udp.send(data);
    std::cout << "完了." << std::endl;
    std::cout << "<ドリブルシーケンス終了>" << std::endl;
  }

  static void ready_for_shoot_action(
      UDP &udp, rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr publisher) {
    std::cout << "<射出シーケンス開始>" << std::endl;
    std::cout << "展開中..." << std::endl;

    data[6] = 1;
    data[8] = 1;
    data[1] = roller_speed_reload;
    data[2] = roller_speed_reload;
    data[3] = -roller_speed_reload;
    data[4] = -roller_speed_reload;
    udp.send(data);

    // 1000msの待機を 100ms のループに分割し、毎回 `stop_requested` を確認

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    udp.send(data);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    data[1] = -roller_speed_shoot_ab;
    data[2] = -roller_speed_shoot_ab;
    data[3] = roller_speed_shoot_cd;
    data[4] = roller_speed_shoot_cd;
    udp.send(data);

    ready_for_shoot = true;
    std::cout << "完了." << std::endl;
  }
};

bool Action::ready_for_shoot = false;

class PS4_Listener : public rclcpp::Node {
public:
  PS4_Listener(const std::string &ip, int port)
      : Node("nhk25_mr"), udp_(ip, port), circle_prev_state(false) {
    subscription_ = this->create_subscription<sensor_msgs::msg::Joy>(
        "/joy", 10,
        std::bind(&PS4_Listener::ps4_listener_callback, this,
                  std::placeholders::_1));
    publisher_ =
        this->create_publisher<std_msgs::msg::Bool>("/backward_signal", 10);
  }

private:
  void ps4_listener_callback(const sensor_msgs::msg::Joy::SharedPtr msg) {
    bool CROSS = msg->buttons[0];
    bool CIRCLE = msg->buttons[1];
    bool TRIANGLE = msg->buttons[2];
    bool PS = msg->buttons[10];
    bool last_cross_state = false;

    if (CROSS && !last_cross_state) {
      stop_requested = true;
      std_msgs::msg::Bool backward_msg;
      backward_msg.data = true;
      publisher_->publish(backward_msg);
    } else {
      stop_requested = false;
      std_msgs::msg::Bool backward_msg;
      backward_msg.data = false;
      publisher_->publish(backward_msg);
    }

    last_cross_state = CROSS;

    if (CIRCLE && !circle_prev_state) {
      stop_requested = false;
      std::thread(&Action::ready_for_shoot_action, std::ref(udp_),
                  std::ref(publisher_))
          .detach();
    }
    circle_prev_state = CIRCLE;

    if (Action::ready_for_shoot) {
      Action::shoot_action(udp_);
    }

    if (TRIANGLE && !Action::ready_for_shoot) {
      Action::dribble_action(udp_);
    }

    udp_.send(data);
  }

  rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr subscription_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr publisher_;
  UDP udp_;
  bool circle_prev_state;
};

class Params_Listener : public rclcpp::Node {
public:
  Params_Listener() : Node("mr_pr_listener") {
    subscription_ = this->create_subscription<std_msgs::msg::Int32MultiArray>(
        "parameter_array", 10,
        std::bind(&Params_Listener::params_listener_callback, this,
                  std::placeholders::_1));
    RCLCPP_INFO(this->get_logger(),

                "NHK2025 Parameter Listener initialized");
  }

private:
  void params_listener_callback(
      const std_msgs::msg::Int32MultiArray::SharedPtr msg) {
    roller_speed_dribble_ab = msg->data[0];
    roller_speed_dribble_cd = msg->data[1];
    roller_speed_shoot_ab = msg->data[2];
    roller_speed_shoot_cd = msg->data[3];
    std::cout << roller_speed_dribble_ab;
    std::cout << roller_speed_dribble_cd;
    std::cout << roller_speed_shoot_ab;
    std::cout << roller_speed_shoot_cd << std::endl;
  }

  rclcpp::Subscription<std_msgs::msg::Int32MultiArray>::SharedPtr subscription_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);

  rclcpp::executors::SingleThreadedExecutor exec;
  auto ps4_listener = std::make_shared<PS4_Listener>(udp_ip, udp_port);
  auto params_listener = std::make_shared<Params_Listener>();
  exec.add_node(ps4_listener);
  exec.add_node(params_listener);

  exec.spin();

  rclcpp::shutdown();
  return 0;
}