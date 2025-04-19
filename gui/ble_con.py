import asyncio
import json
from pathlib import Path
from datetime import datetime
from bleak import BleakScanner, BleakClient
from bleak.backends.winrt.client import BleakClientWinRT
from qasync import asyncSlot
from PySide6.QtCore import Qt  # 新增：导入Qt模块
from PySide6.QtWidgets import QInputDialog, QCheckBox


class DeviceConfigManager:
    """管理设备配置（记住密码、配对状态等）"""
    def __init__(self):
        self.config_path = Path.home() / "ble_device_config.json"
        self.config = self.load_config()  # 格式: {address: {pin, paired, last_connected...}}

    def load_config(self):
        """加载配置文件"""
        if not self.config_path.exists():
            return {}
        try:
            with open(self.config_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"加载配置失败: {str(e)}")
            return {}

    def save_config(self):
        """保存配置文件"""
        try:
            with open(self.config_path, "w", encoding="utf-8") as f:
                json.dump(self.config, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"保存配置失败: {str(e)}")

    def get_device_info(self, address: str) -> dict:
        """获取指定设备的配置信息"""
        return self.config.get(address, {})

    def update_device_info(self, address: str, **kwargs):
        """更新设备配置信息（支持动态字段）"""
        device_info = self.config.get(address, {})
        device_info.update(kwargs)
        self.config[address] = device_info
        self.save_config()


class BleController:
    """BLE功能控制器，处理蓝牙核心逻辑"""
    def __init__(self, main_window):
        self.main_window = main_window  # 引用主窗口实例（用于更新UI）
        self.connected_client = None     # 当前连接的BLE客户端
        self.devices = {}                # 扫描到的设备缓存（address: BleakDevice）
        self.config_manager = DeviceConfigManager()  # 配置管理器
        self.discovered_services = {}    # 发现的服务和特征缓存 {service_uuid: {chars: [...]}}
        self.subscribed_chars = {}       # 已订阅的特征缓存 {char_uuid: {handler, last_raw}}

    async def _get_pin_code(self, address: str) -> tuple:
        """（内部方法）弹出PIN码输入对话框（线程安全）"""
        dialog = QInputDialog(self.main_window)
        dialog.setWindowTitle("输入配对密码")
        dialog.setLabelText(f"请输入设备 {address} 的配对密码（PIN码）:")
        dialog.setTextEchoMode(QInputDialog.Normal)

        # 添加"记住密码"复选框
        remember_checkbox = QCheckBox("记住密码")
        dialog.layout().addWidget(remember_checkbox)

        if dialog.exec() == QInputDialog.Accepted:
            return dialog.textValue().strip(), remember_checkbox.isChecked()
        return None, False  # 用户取消输入

    async def _discover_services_and_chars(self):
        """（内部方法）发现设备的服务和特征，并更新到UI"""
        if not self.connected_client or not self.connected_client.is_connected:
            return

        self.main_window.status_label.setText("状态：正在发现服务和特征...")
        try:
            services = self.connected_client.services  # 获取服务集合
            self.discovered_services.clear()
            self.main_window.list_chars.clear()  # 清空特征列表

            # 遍历每个服务
            for service in services:
                service_info = {
                    "uuid": service.uuid,
                    "name": service.description or "未知服务",
                    "characteristics": []
                }

                # 遍历服务的特征
                for char in service.characteristics:
                    props = char.properties
                    # 仅处理支持通知/指示的特征
                    if "notify" in props or "indicate" in props:
                        char_info = {
                            "uuid": char.uuid,
                            "name": char.description or "未知特征",
                            "properties": props
                        }
                        service_info["characteristics"].append(char_info)
                        # 在UI列表中显示特征
                        display_text = f"{char_info['name']} [{char_info['uuid']}] - 支持: {', '.join(props)}"
                        self.main_window.list_chars.addItem(display_text)
                        # 存储特征UUID到列表项的用户数据中
                        self.main_window.list_chars.item(
                            self.main_window.list_chars.count() - 1
                        ).setData(Qt.UserRole, char_info["uuid"])

                if service_info["characteristics"]:
                    self.discovered_services[service.uuid] = service_info

            # 更新状态标签
            char_count = self.main_window.list_chars.count()
            self.main_window.status_label.setText(
                f"状态：发现 {len(services)} 个服务，其中 {char_count} 个可通知特征"
            )
            self.main_window.btn_subscribe.setEnabled(char_count > 0)

        except Exception as e:
            self.main_window.status_label.setText(f"状态：发现服务失败 - {str(e)}")
            print(f"服务发现错误详情：{e}")

    def _data_parser(self, char_uuid: str, raw_data: bytes) -> str:
        """（内部方法）数据解析函数（可根据设备协议扩展）"""
        # 示例1：假设特征UUID包含"temp"的温度传感器（2字节小端整数，0.1℃单位）
        if "temp" in char_uuid.lower():
            try:
                temp_raw = int.from_bytes(raw_data, byteorder='little', signed=True)
                return f"温度：{temp_raw / 10:.1f}℃"
            except:
                return "温度数据解析失败"

        # 示例2：假设特征UUID包含"hum"的湿度传感器（1字节百分比）
        if "hum" in char_uuid.lower() and len(raw_data) >= 1:
            try:
                return f"湿度：{raw_data[0]}%"
            except:
                return "湿度数据解析失败"

        # 默认返回十六进制字符串
        return f"原始数据：{raw_data.hex()}"

    def _notification_handler(self, char_uuid: str):
        """（内部方法）动态生成的通知回调函数（绑定具体特征）"""
        def handler(sender: int, data: bytes):
            # 存储原始数据到缓存
            self.subscribed_chars[char_uuid]["last_raw"] = data
            # 解析数据
            parsed_data = self._data_parser(char_uuid, data)
            # 触发信号更新UI（通过主窗口的信号）
            self.main_window.data_received.emit(char_uuid, parsed_data)
        return handler

    @asyncSlot()
    async def on_scan_clicked(self):
        """扫描设备按钮点击事件处理"""
        self.main_window.status_label.setText("状态：正在扫描设备...")
        self.main_window.combo_devices.clear()  # 清空设备下拉框
        self.devices.clear()  # 清空设备缓存

        try:
            # 扫描设备（超时5秒）
            scanned_devices = await BleakScanner.discover(timeout=5.0)
            # 去重（按设备地址）
            unique_devices = list({d.address: d for d in scanned_devices}.values())
            # 更新设备列表到UI
            self.main_window.update_device_list(unique_devices)
            self.main_window.status_label.setText(
                f"状态：扫描完成，找到 {len(unique_devices)} 个设备"
            )
        except Exception as e:
            self.main_window.status_label.setText(f"状态：扫描失败 - {str(e)}")
            print(f"扫描错误详情：{e}")

    @asyncSlot()
    async def on_connect_clicked(self):
        """连接设备按钮点击事件处理"""
        current_idx = self.main_window.combo_devices.currentIndex()
        if current_idx == -1:
            self.main_window.status_label.setText("状态：请先选择一个设备")
            return

        # 获取选中设备的地址
        device_address = self.main_window.combo_devices.itemData(current_idx, Qt.UserRole)
        target_device = self.devices.get(device_address)
        if not target_device:
            self.main_window.status_label.setText("状态：设备信息缺失")
            return

        # 初始化连接客户端
        self.connected_client = BleakClient(target_device)
        self.main_window.status_label.setText(f"状态：正在连接 {device_address}...")

        try:
            # 尝试直接连接
            connected = await self.connected_client.connect()

            # 处理需要配对的情况（Windows特有）
            if not connected:
                self.main_window.status_label.setText("状态：需要配对，请求输入PIN码...")
                device_config = self.config_manager.get_device_info(device_address)
                saved_pin = device_config.get("pin")

                # 获取PIN码（优先使用已保存的）
                if saved_pin:
                    pin, remember = saved_pin, True
                else:
                    pin, remember = await self._get_pin_code(device_address)
                    if not pin:  # 用户取消输入
                        self.main_window.status_label.setText("状态：用户取消配对")
                        return

                # 执行配对（仅Windows后端支持）
                if isinstance(self.connected_client._backend, BleakClientWinRT):
                    paired = await self.connected_client.pair(pin_code=pin)
                    if paired:
                        connected = await self.connected_client.connect()
                        # 保存配置（PIN码和配对状态）
                        self.config_manager.update_device_info(
                            device_address,
                            paired=True,
                            pin=pin if remember else None,
                            last_connected=datetime.now().isoformat()
                        )
                    else:
                        raise Exception("配对失败：错误的PIN码或设备不支持")
                else:
                    raise Exception("当前系统不支持自动配对，请手动配对设备")

            # 连接成功后的处理
            if connected:
                # 更新最后连接时间
                self.config_manager.update_device_info(
                    device_address,
                    last_connected=datetime.now().isoformat()
                )
                # 发现服务和特征
                await self._discover_services_and_chars()
                # 更新UI状态
                self.main_window.btn_connect.setEnabled(False)
                self.main_window.btn_disconnect.setEnabled(True)
                self.main_window.status_label.setText(f"状态：已连接到 {device_address}")
            else:
                self.main_window.status_label.setText(f"状态：连接 {device_address} 失败")

        except Exception as e:
            self.main_window.status_label.setText(f"状态：连接错误 - {str(e)}")
            # 清理无效连接
            if self.connected_client:
                await self.connected_client.disconnect()
                self.connected_client = None

    @asyncSlot()
    async def on_disconnect_clicked(self):
        """断开连接按钮点击事件处理"""
        if not self.connected_client:
            self.main_window.status_label.setText("状态：无已连接设备")
            return

        try:
            if self.connected_client.is_connected:
                # 取消所有已订阅的特征通知
                if self.subscribed_chars:
                    self.main_window.status_label.setText("状态：正在取消订阅...")
                    for char_uuid in list(self.subscribed_chars.keys()):
                        try:
                            await self.connected_client.stop_notify(char_uuid)
                        except Exception as e:
                            print(f"取消订阅 {char_uuid} 失败：{str(e)}")
                    self.subscribed_chars.clear()  # 清空订阅缓存
                    # 重置UI按钮状态
                    self.main_window.btn_subscribe.setEnabled(False)
                    self.main_window.btn_unsubscribe.setEnabled(False)
                    self.main_window.list_chars.clear()  # 清空特征列表

                # 断开连接
                await self.connected_client.disconnect()
                self.main_window.status_label.setText("状态：已断开连接")
                # 重置控制器状态
                self.connected_client = None
                self.discovered_services.clear()
                # 可选：清空数据显示区域
                # self.main_window.txt_data.clear()

                # 更新UI按钮状态
                self.main_window.btn_connect.setEnabled(True)
                self.main_window.btn_disconnect.setEnabled(False)
            else:
                self.main_window.status_label.setText("状态：设备已断开")

        except Exception as e:
            self.main_window.status_label.setText(f"状态：断开错误 - {str(e)}")

    @asyncSlot()
    async def on_subscribe_clicked(self):
        """订阅特征按钮点击事件处理"""
        selected_item = self.main_window.list_chars.currentItem()
        if not selected_item:
            self.main_window.status_label.setText("状态：请选择一个特征")
            return

        char_uuid = selected_item.data(Qt.UserRole)
        if char_uuid in self.subscribed_chars:
            self.main_window.status_label.setText("状态：该特征已订阅")
            return

        try:
            # 创建通知回调并订阅
            handler = self._notification_handler(char_uuid)
            await self.connected_client.start_notify(char_uuid, handler)
            # 记录订阅状态
            self.subscribed_chars[char_uuid] = {
                "handler": handler,
                "last_raw": None  # 存储最近一次原始数据
            }
            self.main_window.status_label.setText(f"状态：已订阅特征 {char_uuid}")
            self.main_window.btn_unsubscribe.setEnabled(True)
        except Exception as e:
            self.main_window.status_label.setText(f"状态：订阅失败 - {str(e)}")

    @asyncSlot()
    async def on_unsubscribe_clicked(self):
        """取消订阅按钮点击事件处理"""
        selected_item = self.main_window.list_chars.currentItem()
        if not selected_item:
            self.main_window.status_label.setText("状态：请选择一个特征")
            return

        char_uuid = selected_item.data(Qt.UserRole)
        if char_uuid not in self.subscribed_chars:
            self.main_window.status_label.setText("状态：该特征未订阅")
            return

        try:
            # 停止通知并清理缓存
            await self.connected_client.stop_notify(char_uuid)
            del self.subscribed_chars[char_uuid]
            self.main_window.status_label.setText(f"状态：已取消订阅特征 {char_uuid}")
            # 更新取消订阅按钮状态
            self.main_window.btn_unsubscribe.setEnabled(bool(self.subscribed_chars))
        except Exception as e:
            self.main_window.status_label.setText(f"状态：取消订阅失败 - {str(e)}")
