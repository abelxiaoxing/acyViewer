import sys
import os
import requests
import collections
import base64
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QFileDialog, QDialog, QSpinBox, QLineEdit,
    QFormLayout, QDialogButtonBox, QComboBox, QAction, QMessageBox,
    QStyle
)
from PyQt5.QtGui import QPixmap, QImage, QMovie, QPalette, QColor
from PyQt5.QtCore import Qt, QThread, pyqtSignal, QSettings, QSize, QStandardPaths, QBuffer, QByteArray

# --- 常量定义 ---
KEYBOARD_SHORTCUTS_TIP = "快捷键: A, Space/D, Ctrl+S, Ctrl+,"

# --- 默认设置 ---
DEFAULT_API_URL = "https://www.acy.moe/api/r18"
DEFAULT_MAX_CACHE_SIZE = 5
DEFAULT_THEME = "深色"


# --- 图片获取线程 ---
class ImageFetcher(QThread):
    image_fetched = pyqtSignal(QPixmap, bytes, str)
    fetch_error = pyqtSignal(str)

    def __init__(self, api_url):
        super().__init__()
        self.api_url = api_url
        self.session = requests.Session()
        self.session.headers.update({'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'})
        self._is_interruption_requested = False

    def run(self):
        try:
            response = self.session.get(self.api_url, timeout=20, allow_redirects=True, stream=True)
            response.raise_for_status()

            if self._is_interruption_requested:
                self.fetch_error.emit("下载被中断。")
                return

            image_url = response.url
            image_data_chunks = []
            for chunk in response.iter_content(chunk_size=8192):
                if self._is_interruption_requested:
                    self.fetch_error.emit("下载被中断。")
                    return
                image_data_chunks.append(chunk)
            image_data = b"".join(image_data_chunks)

            if not image_data:
                self.fetch_error.emit("获取到的图片数据为空。")
                return

            pixmap = QPixmap()
            if pixmap.loadFromData(image_data):
                self.image_fetched.emit(pixmap, image_data, image_url)
            else:
                self.fetch_error.emit("无法加载图片数据。可能是无效的图片格式。")
        except requests.exceptions.Timeout:
            self.fetch_error.emit(f"网络请求超时: {self.api_url}")
        except requests.exceptions.RequestException as e:
            self.fetch_error.emit(f"网络请求错误: {e}")
        except Exception as e:
            self.fetch_error.emit(f"获取图片时发生未知错误: {e}")

    def requestInterruption(self):
        self._is_interruption_requested = True

# --- 设置对话框 ---
class SettingsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("设置")
        self.setObjectName("SettingsDialog")
        self.settings = QSettings("MyCompany", "acyViewer")

        layout = QFormLayout(self)
        layout.setSpacing(10)

        self.api_url_edit = QLineEdit(self.settings.value("api_url", DEFAULT_API_URL))
        layout.addRow("API URL:", self.api_url_edit)

        self.cache_size_spinbox = QSpinBox()
        self.cache_size_spinbox.setRange(1, 20)
        self.cache_size_spinbox.setValue(int(self.settings.value("max_cache_size", DEFAULT_MAX_CACHE_SIZE)))
        layout.addRow("最大缓存数量:", self.cache_size_spinbox)

        download_path_layout = QHBoxLayout()
        self.download_dir_edit = QLineEdit(self.settings.value("download_dir", QStandardPaths.writableLocation(QStandardPaths.PicturesLocation)))
        self.browse_button = QPushButton("浏览...")
        self.browse_button.clicked.connect(self.browse_download_dir)
        download_path_layout.addWidget(self.download_dir_edit)
        download_path_layout.addWidget(self.browse_button)
        layout.addRow("默认下载目录:", download_path_layout)

        self.theme_combo = QComboBox()
        self.theme_combo.addItems(["浅色", "深色"])
        current_theme = self.settings.value("theme", DEFAULT_THEME)
        self.theme_combo.setCurrentText(current_theme)
        layout.addRow("主题:", self.theme_combo)

        self.button_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.button_box.accepted.connect(self.accept)
        self.button_box.rejected.connect(self.reject)
        layout.addWidget(self.button_box)

        if parent and parent.styleSheet():
             self.setStyleSheet(parent.styleSheet())

    def browse_download_dir(self):
        directory = QFileDialog.getExistingDirectory(self, "选择下载目录", self.download_dir_edit.text())
        if directory:
            self.download_dir_edit.setText(directory)

    def accept(self):
        self.settings.setValue("api_url", self.api_url_edit.text())
        self.settings.setValue("max_cache_size", self.cache_size_spinbox.value())
        self.settings.setValue("download_dir", self.download_dir_edit.text())
        self.settings.setValue("theme", self.theme_combo.currentText())
        super().accept()

# --- 主窗口 ---
class acyViewer(QMainWindow):
    def __init__(self):
        super().__init__()
        self.settings = QSettings("MyCompany", "acyViewer")

        self.api_url = DEFAULT_API_URL
        self.max_cache_size = DEFAULT_MAX_CACHE_SIZE
        self.download_dir = QStandardPaths.writableLocation(QStandardPaths.PicturesLocation)
        self.current_theme = DEFAULT_THEME

        self.image_cache = collections.deque()
        self.history = collections.deque(maxlen=50)
        self.current_history_index = -1
        self.current_pixmap = None
        self.current_image_data = None
        self.current_image_url = None
        self.fetchers = []

        self.load_settings()
        self.init_ui()
        self.apply_theme()
        self.fill_cache()

    def load_settings(self):
        self.api_url = self.settings.value("api_url", DEFAULT_API_URL)
        self.max_cache_size = int(self.settings.value("max_cache_size", DEFAULT_MAX_CACHE_SIZE))
        self.download_dir = self.settings.value("download_dir", QStandardPaths.writableLocation(QStandardPaths.PicturesLocation))
        if not os.path.isdir(self.download_dir):
            self.download_dir = QStandardPaths.writableLocation(QStandardPaths.PicturesLocation)
            self.settings.setValue("download_dir", self.download_dir)
        self.current_theme = self.settings.value("theme", DEFAULT_THEME)

    def init_ui(self):
        self.setWindowTitle("acy 图片查看器")
        self.setGeometry(100, 100, 850, 650)
        self.setMinimumSize(600, 450)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(15, 15, 15, 15)
        main_layout.setSpacing(10)

        self.image_label = QLabel("正在加载图片...")
        self.image_label.setObjectName("imageLabel")
        self.image_label.setAlignment(Qt.AlignCenter)
        self.image_label.setMinimumSize(600, 400)
        main_layout.addWidget(self.image_label, 1)

        # 直接使用内嵌的base64编码的GIF数据，不依赖外部文件
        gif_data_b64 = "R0lGODlhKAAoAPMAAP///wAAAMLCwkJCQgAAAGJiYoKCgpKSkiH/C05FVFNDQVBFMi4wAwEAAAAh/hpDcmVhdGVkIHdpdGggYWpheGxvYWQuaW5mbwAh+QQJCgAAACwAAAAAKAAoAAAE5xDISWlZKoqzqv27sgvL9LDBVON9GgNAdBGBBCMRHAQCI8E+hHAcgZkYyGCkMatFWq1nI2L3WJ3W4XzBbE4H9p1ZlDEsBsFNrGgDqD7rF+sMDGB0fP85q57a877s/l9+D25vbx1gZ2FnaJljQxAlhZgZl5kBlHAFHAlxKnFnk2luQgpwdGgAn2KdkpafA6IpqKqtrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/////ywAAAAAKAAoAAAE7hDISWlZKoqzqv27sgvL9LDBVON9GgNAdBGBBCMRHAQCI8E+hHAcgZkYyGCkMatFWq1nI2L3WJ3W4XzBbE4H9p1ZlDEsBsFNrGgDqD7rF+sMDGB0fP85q57a877s/l9+D25vbx1gZ2FnaJljQxAlhZgZl5kBlHAFHAlxKnFnk2luQgpwdGgAn2KdkpafA6IpqKqtrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/////wA="
        gif_data = base64.b64decode(gif_data_b64)
        # 使用QBuffer来加载二进制数据到QMovie
        buffer = QBuffer()
        buffer.setData(QByteArray(gif_data))
        buffer.open(QBuffer.ReadOnly)
        self.loading_movie = QMovie()
        self.loading_movie.setDevice(buffer)
        # 保持buffer的引用，防止被垃圾回收
        self.buffer = buffer
        self.loading_movie.setScaledSize(QSize(80,80))

        button_layout = QHBoxLayout()
        button_layout.setSpacing(10)

        # 辅助方法：创建按钮
        def create_button(text, icon_style, tooltip, slot, enabled=True):
            button = QPushButton(text)
            button.setIcon(self.style().standardIcon(icon_style))
            button.setToolTip(tooltip)
            button.clicked.connect(slot)
            button.setEnabled(enabled)
            button_layout.addWidget(button)
            return button

        self.next_button = create_button("下一张", QStyle.SP_MediaSeekForward, 
                                      "查看下一张图片 (Space / D)", self.show_next_image)
        
        self.download_button = create_button("下载", QStyle.SP_DialogSaveButton, 
                                         "下载当前图片 (Ctrl+S)", self.download_current_image, False)
        
        self.copy_button = create_button("复制", QStyle.SP_FileDialogContentsView, 
                                      "复制当前图片到剪贴板", self.copy_image_to_clipboard, False)

        button_layout.addStretch(1)
        main_layout.addLayout(button_layout)

        menubar = self.menuBar()
        
        # 辅助方法：创建菜单项
        def create_menu_action(menu, text, icon_style=None, shortcut=None, status_tip=None, slot=None):
            action = QAction(text, self)
            if icon_style:
                action.setIcon(self.style().standardIcon(icon_style))
            if shortcut:
                action.setShortcut(shortcut)
            if status_tip:
                action.setStatusTip(status_tip)
            if slot:
                action.triggered.connect(slot)
            menu.addAction(action)
            return action
        
        # 文件菜单
        file_menu = menubar.addMenu("文件")
        create_menu_action(file_menu, "设置...", QStyle.SP_FileDialogDetailedView, 
                          "Ctrl+,", "打开设置对话框 (Ctrl+,)", self.open_settings_dialog)
        create_menu_action(file_menu, "退出", QStyle.SP_DialogCloseButton, 
                          "Ctrl+Q", "退出应用程序 (Ctrl+Q)", self.close)
        
        # 导航菜单
        navigate_menu = menubar.addMenu("导航")
        create_menu_action(navigate_menu, "上一张", None, 
                          Qt.Key_A, "查看上一张图片 (A)", self.show_previous_image)
        
        # 下一张菜单项（特殊处理，因为有两个快捷键）
        next_action_menu = create_menu_action(navigate_menu, "下一张 (Space/D)", None, 
                                           Qt.Key_Space, "查看下一张图片 (Space / D)", self.show_next_image)
        # --- 导航菜单结束 ---

        # 更新状态栏初始消息以包含快捷键提示
        self.statusBar().showMessage(f"准备就绪 | {KEYBOARD_SHORTCUTS_TIP}")

    def start_loading_animation(self):
        self.image_label.setMovie(self.loading_movie)
        self.loading_movie.start()
        self.image_label.setAlignment(Qt.AlignCenter)

    def stop_loading_animation(self):
        self.loading_movie.stop()
        self.image_label.setMovie(None)

    def fill_cache(self):
        self.fetchers = [f for f in self.fetchers if f.isRunning()]
        needed = self.max_cache_size - len(self.image_cache) - len(self.fetchers)
        for _ in range(max(0, needed)):
            if len(self.image_cache) + len(self.fetchers) < self.max_cache_size:
                fetcher = ImageFetcher(self.api_url)
                fetcher.image_fetched.connect(self.add_to_cache)
                fetcher.fetch_error.connect(self.handle_fetch_error)
                fetcher.finished.connect(lambda f=fetcher: self.fetchers.remove(f) if f in self.fetchers else None)
                self.fetchers.append(fetcher)
                fetcher.start()

    def add_to_cache(self, pixmap, image_data, image_url):
        if len(self.image_cache) < self.max_cache_size:
            self.image_cache.append((pixmap, image_data, image_url))
            self.statusBar().showMessage(f"缓存成功。缓存: {len(self.image_cache)}/{self.max_cache_size} | {KEYBOARD_SHORTCUTS_TIP}")
        if not self.current_pixmap and self.image_cache:
            self.show_next_image()
        self.fill_cache()

    def handle_fetch_error(self, error_message):
        self.statusBar().showMessage(f"错误: {error_message} | {KEYBOARD_SHORTCUTS_TIP}")
        if not self.current_pixmap and not self.image_cache:
            self.stop_loading_animation()
            self.image_label.setText(f"获取图片失败:\n{error_message[:150]}...")
            self.image_label.setAlignment(Qt.AlignCenter)
        self.fill_cache()

    def display_image(self, pixmap, image_data, image_url):
        self.stop_loading_animation()
        self.current_pixmap = pixmap
        self.current_image_data = image_data
        self.current_image_url = image_url

        scaled_pixmap = pixmap.scaled(self.image_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
        self.image_label.setPixmap(scaled_pixmap)
        self.image_label.setAlignment(Qt.AlignCenter)

        self.download_button.setEnabled(True)
        self.copy_button.setEnabled(True)
        # 状态栏信息可以保持简洁，快捷键提示主要通过ToolTip和菜单
        # self.statusBar().showMessage(f"当前: {image_url}")

    def show_next_image(self):
        if self.current_history_index != -1 and self.current_history_index < len(self.history) - 1:
            self.current_history_index += 1
            pixmap, image_data, img_url = self.history[self.current_history_index]
            self.display_image(pixmap, image_data, img_url)
            self.statusBar().showMessage(f"历史: {self.current_history_index + 1}/{len(self.history)} | {KEYBOARD_SHORTCUTS_TIP}")
        elif self.image_cache:
            pixmap, image_data, img_url = self.image_cache.popleft()
            self.display_image(pixmap, image_data, img_url)

            if self.current_history_index == -1 or \
               (self.current_history_index < len(self.history) -1 and self.history[self.current_history_index+1][2] != img_url) or \
               (self.current_history_index == len(self.history) -1 and (not self.history or self.history[-1][2] != img_url)):
                if self.current_history_index != -1 and self.current_history_index < len(self.history) -1:
                    temp_history = collections.deque(list(self.history)[:self.current_history_index + 1])
                    self.history = temp_history
                self.history.append((pixmap, image_data, img_url))
                self.current_history_index = len(self.history) - 1

            self.statusBar().showMessage(f"显示缓存。缓存: {len(self.image_cache)}/{self.max_cache_size} | {KEYBOARD_SHORTCUTS_TIP}")
            self.fill_cache()
        else:
            self.start_loading_animation()
            self.statusBar().showMessage(f"缓存为空，正在获取新图片... | {KEYBOARD_SHORTCUTS_TIP}")
            self.fill_cache()
            if not self.fetchers and not self.image_cache:
                 QMessageBox.information(self, "提示", "所有图片源尝试失败或缓存为空。\n请检查网络连接和API URL设置。")
                 self.stop_loading_animation()
                 self.image_label.setText("无可用图片。")

    def show_previous_image(self):
        if self.history and self.current_history_index > 0:
            self.current_history_index -= 1
            pixmap, image_data, img_url = self.history[self.current_history_index]
            self.display_image(pixmap, image_data, img_url)
            self.statusBar().showMessage(f"历史: {self.current_history_index + 1}/{len(self.history)} | {KEYBOARD_SHORTCUTS_TIP}")
        elif self.history and self.current_history_index == 0:
             self.statusBar().showMessage(f"已是历史记录第一张。 | {KEYBOARD_SHORTCUTS_TIP}")
        else:
            self.statusBar().showMessage(f"没有更多历史记录。 | {KEYBOARD_SHORTCUTS_TIP}")

    def download_current_image(self):
        if not self.current_image_data or not self.current_image_url:
            QMessageBox.warning(self, "下载失败", "没有当前图片可供下载。")
            return
        try:
            filename_base = os.path.basename(self.current_image_url.split('?')[0])
            filename, ext = os.path.splitext(filename_base)
            if not ext or ext.lower() not in ['.png', '.jpg', '.jpeg', '.gif', '.bmp', '.webp']:
                img_format = QImage.fromData(self.current_image_data).format()
                # 使用字典映射简化格式判断
                format_to_ext = {
                    QImage.Format_PNG: ".png",
                    QImage.Format_JPEG: ".jpg",
                    QImage.Format_GIF: ".gif",
                    QImage.Format_BMP: ".bmp"
                }
                ext = format_to_ext.get(img_format, ".jpg")
            filename = (filename if filename else "image") + ext
        except Exception:
            filename = "image.jpg"

        if not os.path.exists(self.download_dir):
            try:
                os.makedirs(self.download_dir)
            except OSError:
                QMessageBox.critical(self, "错误", f"无法创建下载目录: {self.download_dir}\n将使用默认图片目录。")
                self.download_dir = QStandardPaths.writableLocation(QStandardPaths.PicturesLocation)

        filepath, _ = QFileDialog.getSaveFileName(
            self, "保存图片",
            os.path.join(self.download_dir, filename),
            "图片文件 (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"
        )
        if filepath:
            try:
                with open(filepath, 'wb') as f:
                    f.write(self.current_image_data)
                self.statusBar().showMessage(f"图片已保存到: {filepath} | {KEYBOARD_SHORTCUTS_TIP}")
            except Exception as e:
                QMessageBox.critical(self, "保存失败", f"无法保存图片: {e}")

    def copy_image_to_clipboard(self):
        if self.current_pixmap:
            clipboard = QApplication.clipboard()
            clipboard.setImage(self.current_pixmap.toImage())
            self.statusBar().showMessage(f"图片已复制到剪贴板。 | {KEYBOARD_SHORTCUTS_TIP}")
        else:
            self.statusBar().showMessage(f"没有图片可复制。 | {KEYBOARD_SHORTCUTS_TIP}")

    def keyPressEvent(self, event):
        key = event.key()
        modifiers = event.modifiers()
        # 检查是否是输入框等控件在接收按键，避免冲突
        focused_widget = QApplication.focusWidget()
        if isinstance(focused_widget, (QLineEdit, QSpinBox, QComboBox)):
            super().keyPressEvent(event)
            return

        if key == Qt.Key_Space or key == Qt.Key_D:
            self.show_next_image()
        elif key == Qt.Key_A:
            self.show_previous_image()
        elif modifiers == Qt.ControlModifier and key == Qt.Key_S:
            self.download_current_image()
        # Ctrl+, 和 Ctrl+Q 由 QAction 的快捷键处理，无需在此处重复
        else:
            super().keyPressEvent(event)

    def resizeEvent(self, event):
        if self.current_pixmap:
            scaled_pixmap = self.current_pixmap.scaled(self.image_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation)
            self.image_label.setPixmap(scaled_pixmap)
        super().resizeEvent(event)

    def open_settings_dialog(self):
        dialog = SettingsDialog(self)
        if dialog.exec_():
            old_api_url = self.api_url
            old_max_cache = self.max_cache_size
            old_theme = self.current_theme

            self.load_settings()

            if self.current_theme != old_theme:
                self.apply_theme()

            if self.api_url != old_api_url or self.max_cache_size != old_max_cache:
                self.statusBar().showMessage(f"设置已更新，正在重新初始化缓存... | {KEYBOARD_SHORTCUTS_TIP}")
                for fetcher in list(self.fetchers):
                    if fetcher.isRunning():
                        fetcher.requestInterruption()
                        fetcher.wait(1000)
                self.fetchers.clear()
                self.image_cache.clear()
                self.current_pixmap = None
                self.current_image_data = None
                self.current_image_url = None
                self.download_button.setEnabled(False)
                self.copy_button.setEnabled(False)
                self.start_loading_animation()
                self.fill_cache()
            QMessageBox.information(self, "设置", "设置已保存。")
            # 恢复状态栏的通用提示
            self.statusBar().showMessage(f"设置已保存。缓存: {len(self.image_cache)}/{self.max_cache_size} | {KEYBOARD_SHORTCUTS_TIP}")


    def apply_theme(self):
        palette = QPalette()
        if self.current_theme == "深色":
            palette.setColor(QPalette.Window, QColor(45, 45, 45))
            palette.setColor(QPalette.WindowText, Qt.white)
            palette.setColor(QPalette.Base, QColor(30, 30, 30))
            palette.setColor(QPalette.AlternateBase, QColor(53, 53, 53))
            palette.setColor(QPalette.ToolTipBase, QColor(45, 45, 45))
            palette.setColor(QPalette.ToolTipText, Qt.white)
            palette.setColor(QPalette.Text, Qt.white)
            palette.setColor(QPalette.Button, QColor(60, 60, 60))
            palette.setColor(QPalette.ButtonText, Qt.white)
            palette.setColor(QPalette.BrightText, Qt.red)
            palette.setColor(QPalette.Link, QColor(42, 130, 218))
            palette.setColor(QPalette.Highlight, QColor(42, 130, 218))
            palette.setColor(QPalette.HighlightedText, Qt.black)

            dark_stylesheet = """
                QMainWindow { background-color: #2D2D2D; }
                QLabel#imageLabel { background-color: #1E1E1E; border: 1px solid #4A4A4A; border-radius: 6px; }
                QPushButton { background-color: #4A4A4A; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-size: 10pt; min-height: 28px; }
                QPushButton:hover { background-color: #5A5A5A; }
                QPushButton:pressed { background-color: #3A3A3A; }
                QPushButton:disabled { background-color: #383838; color: #777777; }
                QMenuBar { background-color: #3C3C3C; color: #E0E0E0; }
                QMenuBar::item { background-color: transparent; padding: 4px 8px; }
                QMenuBar::item:selected { background-color: #5A5A5A; }
                QMenu { background-color: #3C3C3C; color: #E0E0E0; border: 1px solid #5A5A5A; padding: 4px; }
                QMenu::item { padding: 4px 20px; }
                QMenu::item:selected { background-color: #5A5A5A; }
                QMenu::separator { height: 1px; background: #5A5A5A; margin-left: 5px; margin-right: 5px; }
                QStatusBar { background-color: #3C3C3C; color: #B0B0B0; }
                QToolTip { color: #E0E0E0; background-color: #5A5A5A; border: 1px solid #6A6A6A; border-radius: 3px; padding: 4px; }
                SettingsDialog { background-color: #2D2D2D; }
                SettingsDialog QLabel { color: #E0E0E0; padding-top: 4px; }
                SettingsDialog QLineEdit, SettingsDialog QSpinBox, SettingsDialog QComboBox {
                    background-color: #3A3A3A; color: #E0E0E0; border: 1px solid #5A5A5A;
                    border-radius: 3px; padding: 5px; min-height: 22px;
                }
                SettingsDialog QSpinBox::up-button, SettingsDialog QSpinBox::down-button {
                    subcontrol-origin: border; background: #4A4A4A; border-radius: 2px;
                }
                SettingsDialog QComboBox::drop-down { border: none; background: #4A4A4A; }
                SettingsDialog QComboBox QAbstractItemView {
                    background-color: #3C3C3C; color: #E0E0E0; border: 1px solid #5A5A5A;
                    selection-background-color: #5A5A5A;
                }
                SettingsDialog QPushButton { min-width: 70px; }
            """
            QApplication.instance().setPalette(palette)
            QApplication.instance().setStyleSheet(dark_stylesheet)
        else:
            QApplication.instance().setPalette(QApplication.style().standardPalette())
            QApplication.instance().setStyleSheet("")

    def closeEvent(self, event):
        for fetcher in self.fetchers:
            if fetcher.isRunning():
                fetcher.requestInterruption()
                fetcher.wait(1500)
        super().closeEvent(event)

if __name__ == '__main__':
    app = QApplication(sys.argv)

    viewer = acyViewer()
    viewer.show()
    sys.exit(app.exec_())