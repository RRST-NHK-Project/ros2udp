# 概要
ROS 2とマイコンをUDP経由で通信するパッケージ群です。現状NUCLEO-F767ZI, Raspberry Pi Picoとの通信を想定しています。

## 各パッケージの詳細
### f7_udp
4輪オムニ試作機、キャチロボ2024で使用。Pythonで記述、C++へ移行のため今後の更新予定なし。
### f7_udp_cpp
f7_udpのC++移植版、NHKロボコン2025で使用予定。
### ros2udp
Picoマイコンへの移行のために書いたやつ、C++で書き換えてf7_udp_cppに統合予定。
## Build & Test Status
### main
[![ROS 2 Jazzy Build & Test](https://github.com/RRST-NHK-Project/ros2udp/actions/workflows/main_jazzy_build_and_test.yml/badge.svg?branch=main)](https://github.com/RRST-NHK-Project/ros2udp/actions/workflows/main_jazzy_build_and_test.yml)  
### develop
[![ROS 2 Jazzy Build & Test](https://github.com/RRST-NHK-Project/ros2udp/actions/workflows/main_jazzy_build_and_test.yml/badge.svg?branch=develop&event=push)](https://github.com/RRST-NHK-Project/ros2udp/actions/workflows/main_jazzy_build_and_test.yml)
