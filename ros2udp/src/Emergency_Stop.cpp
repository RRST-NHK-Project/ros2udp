/*
RRST-NHK-Project 2025
！！！！最重要！！！
複数コントローラーに対応
緊急停止信号をPublishするノード
*/

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "std_msgs/msg/int32_multi_array.hpp"
#include <cmath>
#include <unistd.h>

float passed_range = 30.0; // ±30度の範囲を指定

// MR緊急停止信号送信ノード
class MR_ES_Publisher : public rclcpp::Node {
public:
    MR_ES_Publisher() : Node("MR_ES_Publisher") {
        publisher_ = this->create_publisher<std_msgs::msg::Int32MultiArray>("mr_emergency_stop", 10);
        subscription_ = this->create_subscription<sensor_msgs::msg::Joy>(
            "joy0", 10,
            std::bind(&MR_ES_Publisher::ps4_listener_callback, this,
                      std::placeholders::_1));
        mr_es_msg_.data.resize(1, 0);
    }

private:
    void ps4_listener_callback(const sensor_msgs::msg::Joy::SharedPtr msg) {

        bool PS = msg->buttons[10]; // 緊急停止で使用中！！割り当て禁止
        if (PS) {
            mr_es_msg_.data[0] = int(PS); // PSの入力を垂れ流してるだけ。そのうち復帰機能とかもつけたい。
            publisher_->publish(mr_es_msg_);
            std::cout << "[MR]ソフト緊停" << std::endl;
        }
    }

    rclcpp::Publisher<std_msgs::msg::Int32MultiArray>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr subscription_;
    std_msgs::msg::Int32MultiArray mr_es_msg_;
};

// DR緊急停止信号送信ノード
class DR_ES_Publisher : public rclcpp::Node {
public:
    DR_ES_Publisher() : Node("DR_ES_Publisher") {
        publisher_ = this->create_publisher<std_msgs::msg::Int32MultiArray>("dr_emergency_stop", 10);
        subscription_ = this->create_subscription<sensor_msgs::msg::Joy>(
            "joy1", 10,
            std::bind(&DR_ES_Publisher::ps4_listener_callback, this,
                      std::placeholders::_1));
        dr_es_msg_.data.resize(1, 0);
    }

private:
    void ps4_listener_callback(const sensor_msgs::msg::Joy::SharedPtr msg) {

        bool PS = msg->buttons[10]; // 緊急停止で使用中！！割り当て禁止
        if (PS) {
            dr_es_msg_.data[0] = int(PS); // PSの入力を垂れ流してるだけ。そのうち復帰機能とかもつけたい。
            publisher_->publish(dr_es_msg_);
            std::cout << "[DR]ソフト緊停" << std::endl;
        }
    }

    rclcpp::Publisher<std_msgs::msg::Int32MultiArray>::SharedPtr publisher_;
    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr subscription_;
    std_msgs::msg::Int32MultiArray dr_es_msg_;
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);

    // figletでノード名を表示
    std::string figletout = "figlet SoftES";
    int result = std::system(figletout.c_str());
    if (result != 0) {
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cerr << "Please install 'figlet' with the following command:" << std::endl;
        std::cerr << "sudo apt install figlet" << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    }

    rclcpp::executors::MultiThreadedExecutor exec; // マルチスレッドに変更（意味あるかは知らん）
    auto mr_es_publisher = std::make_shared<MR_ES_Publisher>();
    auto dr_es_publisher = std::make_shared<DR_ES_Publisher>();
    exec.add_node(mr_es_publisher);
    exec.add_node(dr_es_publisher);

    exec.spin();

    rclcpp::shutdown();
    return 0;
}
