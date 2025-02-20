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
#include <custom_srv/srv/fade_away.hpp>
#include <std_msgs/msg/int32_multi_array.hpp>

// 自作クラス
#include "include/UDP.hpp"

// IPアドレスとポートの指定
std::string udp_ip =
    "192.168.8.216"; // 送信先IPアドレス、宛先マイコンで設定したIPv4アドレスを指定
int udp_port = 5000; // 送信元ポート番号、宛先マイコンで設定したポート番号を指定

std::vector<int> data = {0, 0,  0,  0, 0,
                         0, -1, -1, -1}; // 7~9番を電磁弁制御に転用中（-1 or 1）

// 各機構のシーケンスを格納するクラス
// 必要なヘッダファイルのインクルード

// グローバル変数の定義
int roller_speed_dribble_ab = 30;
int roller_speed_dribble_cd = 30;
int roller_speed_shoot_ab = 50;
int roller_speed_shoot_cd = 50;
int roller_speed_reload = 15;

// Actionクラスの定義
class Action {
public:
  static bool ready_for_shoot;
  static bool fadeaway_shoot;

  static void send_reverse_request(rclcpp::Node::SharedPtr node, UDP &udp) {
    auto client =
        node->create_client<custom_srv::srv::FadeAway>("fadeaway_server");
    auto request = std::make_shared<custom_srv::srv::FadeAway::Request>();
    request->reverse = true;

    if (client->wait_for_service(std::chrono::seconds(1))) {
      auto result = client->async_send_request(request);
      RCLCPP_INFO(node->get_logger(), "後退動作をリクエストしました");
    } else {
      RCLCPP_ERROR(node->get_logger(),
                   "後退動作のサービスが見つかりませんでした");
    }
  }

  static void ready_for_shoot_action(UDP &udp) {
    std::cout << "<射出シーケンス開始>" << std::endl;
    std::cout << "展開中..." << std::endl;
    data[6] = 1;
    data[8] = 1;
    data[1] = roller_speed_reload;
    data[2] = roller_speed_reload;
    data[3] = -roller_speed_reload;
    data[4] = -roller_speed_reload;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ready_for_shoot = true;
    std::cout << "完了." << std::endl;
  }

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
  }

  static void dribble_action(UDP &udp) {
    std::cout << "<ドリブルシーケンス開始>" << std::endl;
    data[1] = roller_speed_dribble_ab;
    data[2] = roller_speed_dribble_ab;
    data[3] = -roller_speed_dribble_cd;
    data[4] = -roller_speed_dribble_cd;
    udp.send(data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "ドリブル中" << std::endl;
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
  }
};

bool Action::ready_for_shoot = false;
bool Action::fadeaway_shoot = false;

// PS4 コントローラの入力を受け取るクラス
class PS4_Listener : public rclcpp::Node {
public:
  PS4_Listener(const std::string &ip, int port)
      : Node("nhk25_mr"), udp_(ip, port) {
    subscription_ = this->create_subscription<sensor_msgs::msg::Joy>(
        "joy0", 10,
        std::bind(&PS4_Listener::ps4_listener_callback, this,
                  std::placeholders::_1));
  }

private:
  void ps4_listener_callback(const sensor_msgs::msg::Joy::SharedPtr msg) {
    bool CROSS = msg->buttons[0];
    bool CIRCLE = msg->buttons[1];
    bool TRIANGLE = msg->buttons[2];
    bool PS = msg->buttons[10];

    if (PS) {
      std::fill(data.begin(), data.end(), 0);
      data[6] = data[7] = data[8] = -1;
      udp_.send(data);
      std::cout << "緊急停止！" << std::endl;
      rclcpp::shutdown();
    }

    if (CIRCLE) {
      Action::ready_for_shoot_action(udp_);
      std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }

    if (Action::ready_for_shoot) {
      Action::shoot_action(udp_);
    }

    if (TRIANGLE && !Action::ready_for_shoot) {
      Action::dribble_action(udp_);
    }

    udp_.send(data);
  }

  rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr subscription_;
  UDP udp_;
};

// パラメータリスナー
class Params_Listener : public rclcpp::Node {
public:
  Params_Listener() : Node("mr_pr_listener") {
    subscription_ = this->create_subscription<std_msgs::msg::Int32MultiArray>(
        "parameter_array", 10,
        std::bind(&Params_Listener::params_listener_callback, this,
                  std::placeholders::_1));
  }

private:
  void params_listener_callback(
      const std_msgs::msg::Int32MultiArray::SharedPtr msg) {
    roller_speed_dribble_ab = msg->data[0];
    roller_speed_dribble_cd = msg->data[1];
    roller_speed_shoot_ab = msg->data[2];
    roller_speed_shoot_cd = msg->data[3];
    std::cout << "新しい速度パラメータ: " << roller_speed_dribble_ab << ", "
              << roller_speed_dribble_cd << ", " << roller_speed_shoot_ab
              << ", " << roller_speed_shoot_cd << std::endl;
  }

  rclcpp::Subscription<std_msgs::msg::Int32MultiArray>::SharedPtr subscription_;
};

// メイン関数
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
