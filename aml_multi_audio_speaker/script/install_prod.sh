#!/bin/sh
# 量产时执行该脚本，将config目录下的所有JSON文件拷贝到根文件系统的只读目录
mkdir -p /etc/aml_soundbar/config
cp ./config/*.json /etc/aml_soundbar/config/
chmod 444 /etc/aml_soundbar/config/*.json  # 设置只读权限，防止用户篡改
sync
echo "Config file install success!"