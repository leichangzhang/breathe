# main_window.py
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QComboBox, QPushButton, QLabel,
    QTextEdit, QHBoxLayout, QListWidget
)
from PySide6.QtCore import Qt, Signal, Slot
import datetime

class MainWindow(QWidget):
    data_received = Signal(str, str)

    def __init__(self, ble_controller):
        super().__init__()
        self.ble_controller = ble_controller
        self.init_ui()
        self.setup_signals()

    def init_ui(self):
        self.setWindowTitle("BLE设备管理器")
        self.setGeometry(100, 100, 800, 600)

        layout = QVBoxLayout()

        # 设备选择区域
        self.combo_devices = QComboBox()
        layout.addWidget(self.combo_devices)

        # 操作按钮
        btn_layout = QHBoxLayout()
        self.btn_scan = QPushButton("扫描设备")
        self.btn_connect = QPushButton("连接")
        self.btn_disconnect = QPushButton("断开")
        self.btn_disconnect.setEnabled(False)
        btn_layout.addWidget(self.btn_scan)
        btn_layout.addWidget(self.btn_connect)
        btn_layout.addWidget(self.btn_disconnect)
        layout.addLayout(btn_layout)

        # 状态显示
        self.status_label = QLabel("准备就绪")
        layout.addWidget(self.status_label)

        # 特征列表
        self.list_chars = QListWidget()
        self.list_chars.setSelectionMode(QListWidget.SingleSelection)
        layout.addWidget(QLabel("可订阅特征:"))
        layout.addWidget(self.list_chars)

        # 订阅操作
        sub_btn_layout = QHBoxLayout()
        self.btn_subscribe = QPushButton("订阅")
        self.btn_unsubscribe = QPushButton("取消订阅")
        self.btn_subscribe.setEnabled(False)
        self.btn_unsubscribe.setEnabled(False)
        sub_btn_layout.addWidget(self.btn_subscribe)
        sub_btn_layout.addWidget(self.btn_unsubscribe)
        layout.addLayout(sub_btn_layout)

        # 数据显示
        self.txt_data = QTextEdit()
        self.txt_data.setReadOnly(True)
        layout.addWidget(QLabel("接收数据:"))
        layout.addWidget(self.txt_data)

        self.setLayout(layout)

    def setup_signals(self):
        self.btn_scan.clicked.connect(self.ble_controller.on_scan_clicked)
        self.btn_connect.clicked.connect(self.ble_controller.on_connect_clicked)
        self.btn_disconnect.clicked.connect(self.ble_controller.on_disconnect_clicked)
        self.btn_subscribe.clicked.connect(self.ble_controller.on_subscribe_clicked)
        self.btn_unsubscribe.clicked.connect(self.ble_controller.on_unsubscribe_clicked)
        self.data_received.connect(self.update_data_display)

    @Slot(str, str)
    def update_data_display(self, uuid, data):
        timestamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        raw = self.ble_controller.subscribed_chars.get(uuid, {}).get("last_raw", b"").hex()
        self.txt_data.append(
            f"[{timestamp}] {uuid}\n"
            f"原始: {raw}\n"
            f"解析: {data}\n"
            "----------------------------------------"
        )

    def update_device_list(self, devices):
        self.combo_devices.clear()
        self.ble_controller.devices.clear()

        for device in devices:
            # 确保获取正确的设备属性
            address = device.address
            name = device.name or "未知设备"
            config = self.ble_controller.config_manager.get_device_info(address)

            # 获取设备支持的服务UUID列表
            service_uuids = device.metadata.get("uuids", [])
            service_info = " ".join(["[%s]" % s for s in service_uuids[:2]])  # 显示前两个服务

            # 构建显示文本
            paired = "✓" if config.get("paired") else ""
            item_text = f"{paired} {name} ({address}) {service_info}"

            self.combo_devices.addItem(item_text)
            self.combo_devices.setItemData(self.combo_devices.count() - 1, address, Qt.UserRole)
            self.ble_controller.devices[address] = device

