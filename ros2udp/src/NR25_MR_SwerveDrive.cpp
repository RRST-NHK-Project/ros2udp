/*
RRST-NHK-Project 2025
汎用機独ステ
*/

// 標準
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

// ROS
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "std_msgs/msg/int32.hpp"
#include <std_msgs/msg/int32_multi_array.hpp>

// 自作クラス
#include "include/IP.hpp"
#include "include/UDP.hpp"

#define MC_PRINTF 0 // マイコン側のprintfを無効化・有効化(0 or 1)

// スティックのデッドゾーン
#define DEADZONE_L 0.3
#define DEADZONE_R 0.3

// 定数k
#define k 0.05

// PIDパラメータ（チューニングが必要）
#define speed_Kp 0.3  // 速度Pゲイン
#define speed_Ki 0.05 // 速度Iゲイン
#define speed_Kd 0.05 // 速度Dゲイン

#define speed_limit 30
#define deg_limit 360
#define DPAD_SPEED 30 // 方向パッド入力時の目標速度

bool CHANGEMODE = false;
bool REVERSEMODE = false;

// グローバル変数（角度一覧）
int deg = 0;
int previous_deg = 0;
int truedeg = 0;
int desired_deg = 0;
int measured_deg = 0;

// PID用の内部変数
double speed_Error = 0.0;
double speed_last_Error = 0.0;
double speed_Integral = 0.0;
double speed_Differential = 0.0;
double speed_Output = 0.0;

// 制御周期 [秒]
const double dt = 0.05; // 50ms

// 速度
int wheelspeed = 30;
int yawspeed = 10;
int previous_speed = 0;
int desired_speed = 30;
int measured_speed = 0;
static double current_motor_command = 0.0;

// サーボの組み付け時のズレを補正（度数法）
int SERVO1_CAL = 0;
int SERVO2_CAL = 0;
int SERVO3_CAL = 0;
int SERVO4_CAL = 0;

std::vector<int16_t> data(19, 0); // マイコンに送信される配列"data"
/*
マイコンに送信される配列"data"
debug: マイコンのprintfを有効化, MD: モータードライバー, TR: トランジスタ
| data[n] | 詳細 | 範囲 |
| ---- | ---- | ---- |
| data[0] | debug | 0 or 1 |
| data[1] | MD1 | -100 ~ 100 |
| data[2] | MD2 | -100 ~ 100 |
| data[3] | MD3 | -100 ~ 100 |
| data[4] | MD4 | -100 ~ 100 |
| data[5] | MD5 | -100 ~ 100 |
| data[6] | MD6 | -100 ~ 100 |
| data[7] | Servo1 | 0 ~ 270 |
| data[8] | Servo2 | 0 ~ 270 |
| data[9] | Servo3 | 0 ~ 270 |
| data[10] | Servo4 | 0 ~ 270 |
| data[11] | TR1 | 0 or 1|
| data[12] | TR2 | 0 or 1|
| data[13] | TR3 | 0 or 1|
| data[14] | TR4 | 0 or 1|
| data[15] | TR5 | 0 or 1|
| data[16] | TR6 | 0 or 1|
| data[17] | TR7 | 0 or 1|
| data[18] | TR8 | 0 or 1|
*/

class PS4_Listener : public rclcpp::Node {
public:
    PS4_Listener(const std::string &ip, int port)
        : Node("nhk25_mr_sd"), udp_(ip, port) {
        subscription_ = this->create_subscription<sensor_msgs::msg::Joy>(
            "joy0", 10,
            std::bind(&PS4_Listener::ps4_listener_callback, this,
                      std::placeholders::_1));
        RCLCPP_INFO(this->get_logger(),
                    "NHK2025 MR SD initialized with IP: %s, Port: %d", ip.c_str(),
                    port);
    }

private:
    // コントローラーの入力を取得、使わない入力はコメントアウト推奨
    double PID(int measured_speed) {
        double error = desired_speed - measured_speed;
        double derivative = (error - speed_last_Error) / dt;
        // 積分項の一時更新
        double tentative_integral =
            speed_Integral + (error + speed_last_Error) * dt / 2.0;
        double output = speed_Kp * error + speed_Ki * tentative_integral +
                        speed_Kd * derivative;

        // 変更点：出力が上限・下限を超えた場合、積分更新を抑制（アンチウィンドアップ処理）
        if (output > speed_limit) {
            output = speed_limit;
            if (error > 0) { // 正のエラーの場合、積分更新しない
                tentative_integral = speed_Integral;
            }
        } else if (output < -speed_limit) {
            output = -speed_limit;
            if (error < 0) {
                tentative_integral = speed_Integral;
            }
        }
        speed_Integral = tentative_integral;
        speed_last_Error = error;
        return output;
        // PID計算（台形則による積分計算をそのまま使用）
        // speed_Error = desired_speed - measured_speed;

        // speed_Integral += (speed_Error + speed_last_Error) * dt/ 2.0;
        // speed_Differential = (speed_Error - speed_last_Error) / dt;
        // // 各サンプリングごとにPID出力を再計算
        // speed_Output = (speed_Kp * speed_Error) + (speed_Ki * speed_Integral) +
        // (speed_Kd * speed_Differential); speed_last_Error = speed_Error; return
        // speed_Output;
    }

    void ps4_listener_callback(const sensor_msgs::msg::Joy::SharedPtr msg) {
        float LS_X = -1 * msg->axes[0];
        float LS_Y = msg->axes[1];
        float RS_X = -1 * msg->axes[3];
        // float RS_Y = msg->axes[4];

        bool CROSS = msg->buttons[0];
        // bool CIRCLE = msg->buttons[1];
        // bool TRIANGLE = msg->buttons[2];
        // bool SQUARE = msg->buttons[3];

        bool LEFT = msg->axes[6] == 1.0;
        bool RIGHT = msg->axes[6] == -1.0;
        bool UP = msg->axes[7] == 1.0;
        bool DOWN = msg->axes[7] == -1.0;

        // bool L1 = msg->buttons[4];
        bool R1 = msg->buttons[5];

        // float L2 = (-1 * msg->axes[2] + 1) / 2;
        float R2 = (-1 * msg->axes[5] + 1) / 2;

        bool SHARE = msg->buttons[8];
        bool OPTION = msg->buttons[9];
        bool PS = msg->buttons[10];
        static bool last_option = false; // 前回の状態を保持する static 変数
        static bool last_share = false;
        // OPTION のラッチ状態を保持する static 変数（初期状態は OFF とする）
        static bool option_latch = false;
        static bool share_latch = false;

        // bool L3 = msg->buttons[11];
        // bool R3 = msg->buttons[12];

        data[0] = MC_PRINTF; // マイコン側のprintfを無効化・有効化(0 or 1)

        if (PS) {
            std::fill(data.begin(), data.end(), 0);          // 配列をゼロで埋める
            for (int attempt = 0; attempt < 10; attempt++) { // 10回試行
                udp_.send(data);                             // データ送信
                std::cout << "緊急停止！ 試行" << attempt + 1
                          << std::endl; // 試行回数を表示
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100)); // 100msの遅延
            }
            rclcpp::shutdown();
        }

        float rad = atan2(LS_Y, LS_X);
        deg = rad * 180 / M_PI;
        if (OPTION && !last_option) {
            option_latch = !option_latch;
        }
        if (SHARE && !last_share) {
            share_latch = !share_latch;
        }
        // ！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
        // もとの移動方法！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
        // ！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
        if (CHANGEMODE == 0) {
            // XY座標での正しい角度truedeg
            if (REVERSEMODE == 0) {
                truedeg = deg;
                if ((0 <= truedeg) && (truedeg <= 180)) {
                    truedeg = truedeg;
                }
                if ((-180 <= truedeg) && (truedeg <= 0)) {
                    truedeg = -truedeg + 360;
                }

                // ！！！！！最重要！！！！！
                //  XY座標での９０度の位置に１３５度を変換して計算
                if ((-180 <= deg) && (deg <= -135)) {
                    deg = -deg - 135;
                } else {
                    deg = 225 - deg;
                }

                // deadzone追加
                if ((fabs(LS_X) <= DEADZONE_R) && (fabs(LS_Y) <= DEADZONE_R) &&
                    (fabs(RS_X) <= DEADZONE_L)) {
                    deg = 135;
                    data[1] = 0;
                    data[2] = 0;
                    data[3] = 0;
                    data[4] = 0;
                    data[7] = deg + SERVO1_CAL;
                    data[8] = deg + SERVO2_CAL;
                    data[9] = deg + SERVO3_CAL;
                    data[10] = deg + SERVO4_CAL;
                }

                data[1] = -wheelspeed * R2;
                data[2] = -wheelspeed * R2;
                data[3] = -wheelspeed * R2;
                data[4] = -wheelspeed * R2;
                data[7] = deg + SERVO1_CAL;
                data[8] = deg + SERVO2_CAL;
                data[9] = deg + SERVO3_CAL;
                data[10] = deg + SERVO4_CAL;

                if (LEFT) {
                    deg = 45;
                    data[1] = -wheelspeed * R2;
                    data[2] = -wheelspeed * R2;
                    data[3] = -wheelspeed * R2;
                    data[4] = -wheelspeed * R2;
                }
                if (RIGHT) {
                    deg = 45;
                    data[1] = wheelspeed * R2;
                    data[2] = wheelspeed * R2;
                    data[3] = wheelspeed * R2;
                    data[4] = wheelspeed * R2;
                }
                if (UP) {
                    deg = 135;
                    data[1] = -wheelspeed * R2;
                    data[2] = -wheelspeed * R2;
                    data[3] = -wheelspeed * R2;
                    data[4] = -wheelspeed * R2;
                }
                if (DOWN) {
                    deg = 135;
                    data[1] = wheelspeed * R2;
                    data[2] = wheelspeed * R2;
                    data[3] = wheelspeed * R2;
                    data[4] = wheelspeed * R2;
                }

                // 独ステが扱えない範囲の変換
                if ((270 < deg) && (deg < 360)) {
                    deg = deg - 180;
                    data[1] = wheelspeed * R2;
                    data[2] = wheelspeed * R2;
                    data[3] = wheelspeed * R2;
                    data[4] = wheelspeed * R2;
                    data[7] = deg + SERVO1_CAL;
                    data[8] = deg + SERVO2_CAL;
                    data[9] = deg + SERVO3_CAL;
                    data[10] = deg + SERVO4_CAL;
                }

                // 時計回りYAW回転
                if (RS_X < 0 && fabs(RS_X) >= DEADZONE_R) {
                    data[7] = 180 + SERVO1_CAL;
                    data[8] = 90 + SERVO2_CAL;
                    data[9] = 90 + SERVO3_CAL;
                    data[10] = 180 + SERVO4_CAL;
                    data[1] = yawspeed;
                    data[2] = -yawspeed;
                    data[3] = yawspeed;
                    data[4] = -yawspeed;
                }
                // 半時計回りYAW回転
                if (0 < RS_X && fabs(RS_X) >= DEADZONE_R) {
                    data[7] = 180 + SERVO1_CAL;
                    data[8] = 90 + SERVO2_CAL;
                    data[9] = 90 + SERVO3_CAL;
                    data[10] = 180 + SERVO4_CAL;
                    data[1] = -yawspeed;
                    data[2] = yawspeed;
                    data[3] = -yawspeed;
                    data[4] = yawspeed;
                }
            }
            // 反転モード
            //
            if (REVERSEMODE == 1) {
                truedeg = deg;
                if ((0 <= truedeg) && (truedeg <= 180)) {
                    truedeg = truedeg;
                }
                if ((-180 <= truedeg) && (truedeg <= 0)) {
                    truedeg = -truedeg + 360;
                }

                // ！！！！！最重要！！！！！
                //  XY座標での９０度の位置に１３５度を変換して計算
                if ((-180 <= deg) && (deg <= -135)) {
                    deg = -deg - 135;
                } else {
                    deg = 225 - deg;
                }

                // deadzone追加
                if ((fabs(LS_X) <= DEADZONE_R) && (fabs(LS_Y) <= DEADZONE_R) &&
                    (fabs(RS_X) <= DEADZONE_L)) {
                    deg = 135;
                    data[1] = 0;
                    data[2] = 0;
                    data[3] = 0;
                    data[4] = 0;
                    data[7] = deg + SERVO1_CAL;
                    data[8] = deg + SERVO2_CAL;
                    data[9] = deg + SERVO3_CAL;
                    data[10] = deg + SERVO4_CAL;
                }

                data[1] = wheelspeed * R2;
                data[2] = wheelspeed * R2;
                data[3] = wheelspeed * R2;
                data[4] = wheelspeed * R2;
                data[7] = deg + SERVO1_CAL;
                data[8] = deg + SERVO2_CAL;
                data[9] = deg + SERVO3_CAL;
                data[10] = deg + SERVO4_CAL;

                if (LEFT) {
                    deg = 45;
                    data[1] = wheelspeed * R2;
                    data[2] = wheelspeed * R2;
                    data[3] = wheelspeed * R2;
                    data[4] = wheelspeed * R2;
                }
                if (RIGHT) {
                    deg = 45;
                    data[1] = -wheelspeed * R2;
                    data[2] = -wheelspeed * R2;
                    data[3] = -wheelspeed * R2;
                    data[4] = -wheelspeed * R2;
                }
                if (UP) {
                    deg = 135;
                    data[1] = wheelspeed * R2;
                    data[2] = wheelspeed * R2;
                    data[3] = wheelspeed * R2;
                    data[4] = wheelspeed * R2;
                }
                if (DOWN) {
                    deg = 135;
                    data[1] = -wheelspeed * R2;
                    data[2] = -wheelspeed * R2;
                    data[3] = -wheelspeed * R2;
                    data[4] = -wheelspeed * R2;
                }

                // 射出直前にサーボを直角に向けストップ
                if (CROSS) {
                    data[1] = 0;
                    data[2] = 0;
                    data[3] = 0;
                    data[4] = 0;
                    data[5] = 0;
                    data[6] = 0;
                    data[7] = 45 + SERVO1_CAL;
                    data[8] = 45 + SERVO2_CAL;
                    data[9] = 45 + SERVO3_CAL;
                    data[10] = 45 + SERVO4_CAL;
                }

                // 独ステが扱えない範囲の変換
                if ((270 < deg) && (deg < 360)) {
                    deg = deg - 180;
                    data[1] = -wheelspeed * R2;
                    data[2] = -wheelspeed * R2;
                    data[3] = -wheelspeed * R2;
                    data[4] = -wheelspeed * R2;
                    data[7] = deg + SERVO1_CAL;
                    data[8] = deg + SERVO2_CAL;
                    data[9] = deg + SERVO3_CAL;
                    data[10] = deg + SERVO4_CAL;
                }

                // 時計回りYAW回転
                if (RS_X < 0 && fabs(RS_X) >= DEADZONE_R) {
                    data[7] = 180 + SERVO1_CAL;
                    data[8] = 90 + SERVO2_CAL;
                    data[9] = 90 + SERVO3_CAL;
                    data[10] = 180 + SERVO4_CAL;
                    data[1] = yawspeed;
                    data[2] = -yawspeed;
                    data[3] = yawspeed;
                    data[4] = -yawspeed;
                }
                // 半時計回りYAW回転
                if (0 < RS_X && fabs(RS_X) >= DEADZONE_R) {
                    data[7] = 180 + SERVO1_CAL;
                    data[8] = 90 + SERVO2_CAL;
                    data[9] = 90 + SERVO3_CAL;
                    data[10] = 180 + SERVO4_CAL;
                    data[1] = -yawspeed;
                    data[2] = yawspeed;
                    data[3] = -yawspeed;
                    data[4] = yawspeed;
                }
            }
            last_share = SHARE;
            REVERSEMODE = share_latch;
            // std::cout << REVERSEMODE << std::endl;
        }
        // ！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
        // 加速する移動方法！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
        // ！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
        if (CHANGEMODE == 1) {
            speed_Output = -PID(measured_speed);

            // XY座標での正しい角度truedeg
            truedeg = deg;
            if ((0 <= truedeg) && (truedeg <= 180)) {
                truedeg = truedeg;
            }
            if ((-180 <= truedeg) && (truedeg <= 0)) {
                truedeg = -truedeg + 360;
            }

            // ！！！！！最重要！！！！！
            //  XY座標での９０度の位置に１３５度を変換して計算
            if ((-180 <= deg) && (deg <= -135)) {
                deg = -deg - 135;
            } else {
                deg = 225 - deg;
            }

            data[1] = speed_Output;
            data[2] = speed_Output;
            data[3] = speed_Output;
            data[4] = speed_Output;

            if (LEFT) {
                deg = 45;
                speed_Output = speed_Output;
                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
            }
            if (RIGHT) {
                deg = 45;
                speed_Output = -speed_Output;
                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
            }
            if (UP) {
                deg = 135;
                speed_Output = speed_Output;
                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
            }
            if (DOWN) {
                deg = 135;
                speed_Output = -speed_Output;
                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
            }

            // 射出直前にサーボを直角に向けストップ
            if (CROSS) {
                data[1] = 0;
                data[2] = 0;
                data[3] = 0;
                data[4] = 0;
                data[5] = 0;
                data[6] = 0;
                data[7] = 45 + SERVO1_CAL;
                data[8] = 45 + SERVO2_CAL;
                data[9] = 45 + SERVO3_CAL;
                data[10] = 45 + SERVO4_CAL;
            }

            // 後ろに下がる動き
            if ((225 < deg) && (deg <= 360) && (R1)) {
                deg = deg - 180;

                data[7] = deg + SERVO1_CAL;
                data[8] = deg + SERVO2_CAL;
                data[9] = deg + SERVO3_CAL;
                data[10] = deg + SERVO4_CAL;
                speed_Output = -speed_Output;
                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
            }
            if ((0 <= deg) && (deg < 45) && (R1)) {
                deg = deg + 180;
                data[7] = deg + SERVO1_CAL;
                data[8] = deg + SERVO2_CAL;
                data[9] = deg + SERVO3_CAL;
                data[10] = deg + SERVO4_CAL;
                speed_Output = -speed_Output;
                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
            }

            data[7] = deg + SERVO1_CAL;
            data[8] = deg + SERVO2_CAL;
            data[9] = deg + SERVO3_CAL;
            data[10] = deg + SERVO4_CAL;

            previous_deg = desired_deg;

            // 時計回りYAW回転
            if (RS_X < 0 && fabs(RS_X) >= DEADZONE_R) {
                speed_Output = -yawspeed;
                data[7] = 180 + SERVO1_CAL;
                data[8] = 90 + SERVO2_CAL;
                data[9] = 90 + SERVO3_CAL;
                data[10] = 180 + SERVO4_CAL;
                data[1] = speed_Output;
                data[2] = -speed_Output;
                data[3] = speed_Output;
                data[4] = -speed_Output;
            }
            // 半時計回りYAW回転
            if (0 < RS_X && fabs(RS_X) >= DEADZONE_R) {
                speed_Output = yawspeed;
                data[7] = 180 + SERVO1_CAL;
                data[8] = 90 + SERVO2_CAL;
                data[9] = 90 + SERVO3_CAL;
                data[10] = 180 + SERVO4_CAL;
                data[1] = speed_Output;
                data[2] = -speed_Output;
                data[3] = speed_Output;
                data[4] = -speed_Output;
            }
            // deadzone追加
            if ((fabs(LS_X) <= DEADZONE_R) && (fabs(LS_Y) <= DEADZONE_R) &&
                (fabs(RS_X) <= DEADZONE_L) && (!LEFT && !RIGHT && !UP && !DOWN)) {
                deg = 135;
                desired_speed = 0;
                measured_speed = 0;

                speed_Integral = 0;
                speed_Differential = 0;

                double decay_rate = 0.05; // 0～1の値。大きいほど速く0に近づく

                if (previous_speed > 0) {
                    current_motor_command += (0 - current_motor_command) * decay_rate;
                    speed_Output = current_motor_command;
                } else if (previous_speed < 0) {
                    current_motor_command += (0 - current_motor_command) * decay_rate;
                    speed_Output = current_motor_command;
                }

                data[1] = speed_Output;
                data[2] = speed_Output;
                data[3] = speed_Output;
                data[4] = speed_Output;
                data[7] = deg + SERVO1_CAL;
                data[8] = deg + SERVO2_CAL;
                data[9] = deg + SERVO3_CAL;
                data[10] = deg + SERVO4_CAL;
            } else {
                desired_speed = 30;
                current_motor_command = speed_Output;
            }

            previous_speed = speed_Output;
            previous_deg = deg;
            // std::cout << data[1] << std::endl;
        }

        // デバッグ用（for文でcoutするとカクつく）
        // std::cout << data[0] << ", " << data[1] << ", " << data[2] << ", " <<
        // data[3] << ", "; std::cout << data[4] << ", " << data[5] << ", " <<
        // data[6] << ", " << data[7] << ", "; std::cout << data[8] << ", " <<
        // data[9] << ", " << data[10] << ", " << data[11] << ", "; std::cout <<
        // data[12] << ", " << data[13] << ", " << data[14] << ", " << data[15] <<
        // ", "; std::cout << data[16] << ", " << data[17] << ", " << data[18] <<
        // std::endl;

        // 現在の状態を次回のために保存
        last_option = OPTION;
        // last_share = SHARE;
        CHANGEMODE = option_latch;
        // REVERSEMODE = share_latch;
        // std::cout << data[1] << ", " << data[2] << ", " << data[3] << ", " <<
        // data[4]<< std::endl;
        //   std::cout << data[1] << ", " << speed_Output << ", " << speed_Integral
        //   << ", " << std::endl;
        udp_.send(data);
    }

    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr subscription_;
    UDP udp_;
};

class Servo_Deg_Publisher : public rclcpp::Node {
public:
    Servo_Deg_Publisher() : Node("mr_servo_deg_publisher") {
        // Publisherの作成
        publisher_ = this->create_publisher<std_msgs::msg::Int32MultiArray>(
            "mr_servo_deg", 10);

        // タイマーを使って定期的にメッセージをpublish
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(10),
            std::bind(&Servo_Deg_Publisher::publish_message, this));
    }

private:
    void publish_message() {
        auto message = std_msgs::msg::Int32MultiArray();
        message.data = {data[5], data[6], data[7], data[8]};

        // RCLCPP_INFO(this->get_logger(), "Publishing: '%d'", message.data);
        publisher_->publish(message); // メッセージをpublish
    }

    rclcpp::Publisher<std_msgs::msg::Int32MultiArray>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

class Params_Listener : public rclcpp::Node {
public:
    Params_Listener() : Node("nr25_mr_servo_cal_listener") {
        subscription_ = this->create_subscription<std_msgs::msg::Int32MultiArray>(
            "mr_servo_cal", 10,
            std::bind(&Params_Listener::params_listener_callback, this,
                      std::placeholders::_1));
        RCLCPP_INFO(this->get_logger(), "MR Servo Calibrator Listener");
    }

private:
    void params_listener_callback(
        const std_msgs::msg::Int32MultiArray::SharedPtr msg) {
        SERVO1_CAL = msg->data[0];
        SERVO2_CAL = msg->data[1];
        SERVO3_CAL = msg->data[2];
        SERVO4_CAL = msg->data[3];
    }

    rclcpp::Subscription<std_msgs::msg::Int32MultiArray>::SharedPtr subscription_;
};

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);

    // figletでノード名を表示
    std::string figletout = "figlet MR SwerveDrive";
    int result = std::system(figletout.c_str());
    if (result != 0) {
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                  << std::endl;
        std::cerr << "Please install 'figlet' with the following command:"
                  << std::endl;
        std::cerr << "sudo apt install figlet" << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                  << std::endl;
    }

    rclcpp::executors::SingleThreadedExecutor exec;
    auto ps4_listener = std::make_shared<PS4_Listener>(IP_MR_SD, PORT_MR_SD);
    auto servo_deg_publisher = std::make_shared<Servo_Deg_Publisher>();
    auto params_listener = std::make_shared<Params_Listener>();
    exec.add_node(ps4_listener);
    exec.add_node(servo_deg_publisher);
    exec.add_node(params_listener);

    exec.spin();

    rclcpp::shutdown();
    return 0;
}