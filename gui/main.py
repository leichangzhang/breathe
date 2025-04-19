# main.py
import sys
import asyncio
from qasync import QEventLoop
from PySide6.QtWidgets import QApplication
from main_window import MainWindow
from ble_con import BleController


def main():
    app = QApplication(sys.argv)
    loop = QEventLoop(app)
    asyncio.set_event_loop(loop)

    ble_controller = BleController(main_window=None)
    main_window = MainWindow(ble_controller)
    ble_controller.main_window = main_window

    main_window.show()

    with loop:
        loop.run_forever()


if __name__ == "__main__":
    main()
