#!/usr/bin/env python3
## coding: UTF-8

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy
from std_msgs.msg import Float64

from rclpy.executors import SingleThreadedExecutor

from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy

from socket import *
import time
import math

# 以下pipでのインストールが必要
import pyfiglet
from simple_pid import PID

data = [0, 0, 0, 0, 0, 0, 0, 0, 0]  # 各モーターの回転数(RPM)を格納する


# --------------------------#
# オムニのパラメーター
fth = 0
vth = 0
r = 0
rpm_limit = 20
sp_yaw = 0.5
sp_omni = 1.0
global dir_fix
# --------------------------#

deadzone = 0.3  # Joyスティックのデッドゾーンを設定
imu_deadzone = 0.01

online_mode = True  # ルーターと接続せずに実行したいときはFalseにする

pid = PID(
    2.0,  # Kp
    0.1,  # Ki
    0.0,  # Kd
    setpoint=0,  # target
    # sample_time=0.01,
    output_limits=(-1.0, 1.0),
    auto_mode=True,
)


class IMU_Listener(Node):

    def __init__(self):
        super().__init__("nhk25_omni_driver_imu_listener")

        qos_profile = QoSProfile(  # imuのノードがROS1から移植されたものなのでQoSプロファイルを合わせる必要がある
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
        )

        self.subscription = self.create_subscription(
            Float64, "/wit_ros/related_yaw", self.listener_callback, qos_profile
        )
        self.subscription  # prevent unused variable warning

    def listener_callback(self, imu_msg):
        global dir_fix
        related_yow = imu_msg.data
        # print(related_yow * 180 / math.pi)
        dir_fix = pid(related_yow)
        if math.fabs(dir_fix) < imu_deadzone:
            dir_fix = 0
        # print(dir_fix)
        if online_mode == True:
            udp.send()  # 関数実行


class PS4_Listener(Node):

    def __init__(self):
        super().__init__("nhk25_omni_driver_ps4_listener")
        self.subscription = self.create_subscription(Joy, "joy", self.ps4_callback, 10)
        print(pyfiglet.figlet_format("NHK2025"))
        self.subscription  # prevent unused variable warning
        global dir_fix

    def ps4_callback(self, ps4_msg):

        global dir_fix

        LS_X = (-1) * ps4_msg.axes[0]
        LS_Y = ps4_msg.axes[1]
        RS_X = (-1) * ps4_msg.axes[2]
        RS_Y = ps4_msg.axes[5]

        # print(LS_X, LS_Y, RS_X, RS_Y)

        CROSS = ps4_msg.buttons[1]
        CIRCLE = ps4_msg.buttons[2]
        TRIANGLE = ps4_msg.buttons[3]
        SQUARE = ps4_msg.buttons[0]

        LEFT = ps4_msg.axes[12] == 1.0
        RIGHT = ps4_msg.axes[12] == -1.0
        UP = ps4_msg.axes[13] == 1.0
        DOWN = ps4_msg.axes[13] == -1.0

        L1 = ps4_msg.buttons[4]
        R1 = ps4_msg.buttons[5]

        L2 = ps4_msg.buttons[6]
        R2 = ps4_msg.buttons[7]

        SHARE = ps4_msg.buttons[8]
        OPTION = ps4_msg.buttons[9]
        PS = ps4_msg.buttons[12]

        L3 = ps4_msg.buttons[10]
        R3 = ps4_msg.buttons[11]

        if PS:
            print(pyfiglet.figlet_format("HALT"))
            data[1] = 0
            data[2] = 0
            data[3] = 0
            data[4] = 0
            data[5] = 0
            data[6] = 0
            data[7] = 0
            data[8] = 0
            if online_mode == True:
                udp.send()  # 関数実行
            time.sleep(1)
            while True:
                pass

        if UP == 1:
            LS_Y = 1.0

        if DOWN == 1:
            LS_Y = -1.0

        if LEFT == 1:
            LS_X = -1.0

        if RIGHT == 1:
            LS_X = 1.0

        rad = math.atan2(LS_Y, LS_X)
        # vx = math.cos(rad)
        # vy = math.sin(rad)

        v1 = math.sin(rad - 1 * math.pi / 4) * sp_omni
        v2 = math.sin(rad - 3 * math.pi / 4) * sp_omni
        v3 = math.sin(rad - 5 * math.pi / 4) * sp_omni
        v4 = math.sin(rad - 7 * math.pi / 4) * sp_omni

        if RS_X >= deadzone:
            R2 = 1

        if RS_X <= -1 * deadzone:
            L2 = 1

        if R2 == 1:
            v1 = -1.0 * sp_yaw
            v2 = -1.0 * sp_yaw
            v3 = -1.0 * sp_yaw
            v4 = -1.0 * sp_yaw

        if L2 == 1:
            v1 = 1.0 * sp_yaw
            v2 = 1.0 * sp_yaw
            v3 = 1.0 * sp_yaw
            v4 = 1.0 * sp_yaw

        if (
            (math.fabs(LS_X) <= deadzone)
            and (math.fabs(LS_Y) <= deadzone)
            and R2 == 0
            and L2 == 0
        ):
            v1 = 0
            v2 = 0
            v3 = 0
            v4 = 0

        # print(v1, v2, v3, v4)
        data[1] = v1 * (rpm_limit + 1) + (dir_fix * rpm_limit)
        data[2] = v2 * (rpm_limit + 1) + (dir_fix * rpm_limit)
        data[3] = v3 * (rpm_limit + 1) + (dir_fix * rpm_limit)
        data[4] = v4 * (rpm_limit + 1) + (dir_fix * rpm_limit)

        print(data[1], data[2], data[3], data[4])
        # print(dir_fix)

        if online_mode == True:
            udp.send()  # 関数実行


class udpsend:
    def __init__(self):

        SrcIP = "192.168.8.195"  # 送信元IP
        SrcPort = 4000  # 送信元ポート番号
        self.SrcAddr = (SrcIP, SrcPort)  # アドレスをtupleに格納

        DstIP = "192.168.8.215"  # 宛先IP
        DstPort = 5000  # 宛先ポート番号
        self.DstAddr = (DstIP, DstPort)  # アドレスをtupleに格納

        self.udpClntSock = socket(AF_INET, SOCK_DGRAM)  # ソケット作成
        self.udpClntSock.bind(self.SrcAddr)  # 送信元アドレスでバインド

    def send(self):

        # print(data[1], data[2], data[3], data[4])

        str_data = (
            str(data[1])
            + ","
            + str(data[2])
            + ","
            + str(data[3])
            + ","
            + str(data[4])
            + ","
            + str(data[5])
            + ","
            + str(data[6])
            + ","
            + str(data[7])
            + ","
            + str(data[8])
        )  # パケットを作成

        send_data = str_data.encode("utf-8")  # バイナリに変換

        self.udpClntSock.sendto(send_data, self.DstAddr)  # 宛先アドレスに送信

        data[1] = 0
        data[2] = 0
        data[3] = 0
        data[4] = 0
        data[5] = 0
        data[6] = 0
        data[7] = 0
        data[8] = 0


if online_mode == True:
    udp = udpsend()  # クラス呼び出し


def main(args=None):
    rclpy.init(args=args)
    exec = SingleThreadedExecutor()

    imu_listener = IMU_Listener()
    ps4_listener = PS4_Listener()

    exec.add_node(imu_listener)
    exec.add_node(ps4_listener)

    exec.spin()

    # Destroy the node explicitly
    # (optional - otherwise it will be done automatically
    # when the garbage collector destroys the node object)
    imu_listener.destroy_node()
    ps4_listener.destroy_node()
    exec.shutdown()
    # ser.close


if __name__ == "__main__":
    main()
