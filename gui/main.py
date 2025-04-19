
import sys
import os
from PySide6.QtWidgets import QApplication, QMainWindow, QPushButton,QWidget
from PySide6.QtGui import QPainter, QPixmap
from PySide6.QtCore import Qt,QThread,Signal,Slot
from PySide6 import QtWidgets, QtGui, QtCore
from ui.mainwindow import Ui_MainWindow
import datetime
import time


class MainWindow(QMainWindow):
    def __init__(self):
        super(MainWindow, self).__init__()
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)
        self.move(400, 200)
        self.connect_signal()
    def connect_signal(self):
        pass


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
