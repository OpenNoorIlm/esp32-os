import sys
sys.path.insert(0, r"C:\Users\bismi\Downloads\noorRobot-system\esp32-ssh\packages\shell")
sys.path.insert(0, r"C:\Users\bismi\Downloads\noorRobot-system\esp32-ssh\apps\esp32-app")
from PyQt5.QtWidgets import QApplication
from PyQt5.QtTest import QTest
from PyQt5.QtCore import Qt, QTimer
import main as app_mod

orig_refresh = app_mod.WifiTab._refresh
def patched_refresh(self):
    print("WIFI REFRESH CALLED", flush=True)
    orig_refresh(self)
app_mod.WifiTab._refresh = patched_refresh

app = QApplication(sys.argv)
app.setStyleSheet(app_mod.APP_STYLESHEET)
win = app_mod.MainWindow()
win.show()

def click_refresh():
    sub = win._open_windows["wifi"]
    content = sub.widget()
    wifi_tab = content.findChild(app_mod.WifiTab)
    btns = wifi_tab.findChildren(app_mod.ToolIconButton)
    print("Found buttons:", len(btns), flush=True)
    for b in btns:
        print("  button tooltip:", b.toolTip(), "visible:", b.isVisible(), "enabled:", b.isEnabled(), flush=True)
    if btns:
        QTest.mouseClick(btns[0], Qt.LeftButton)
    QTimer.singleShot(1500, app.quit)

def do_test():
    win._open_app("wifi")
    QTimer.singleShot(600, click_refresh)

QTimer.singleShot(600, do_test)
app.exec_()
print("DONE", flush=True)
