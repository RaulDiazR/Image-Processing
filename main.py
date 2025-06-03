import sys
import os
import re
from PyQt5 import QtWidgets, QtGui, QtCore
from gui5 import Ui_MainWindow

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)

        # Resize window so three 120px images fit without horizontal scroll
        self.resize(800, 600)

        self.ui.seleccionarCarpetaEntrada.clicked.connect(self.seleccionar_entrada)
        self.ui.seleccionarCarpetaSalida.clicked.connect(self.seleccionar_salida)
        self.ui.processImages.clicked.connect(self.procesar)
        self.ui.generateReport.clicked.connect(self.generar_txt)
        self.ui.actionMostrarIntegrantes.triggered.connect(self.mostrar_integrantes)
        self.ui.kernel_slider.valueChanged.connect(self.actualizar_valor_kernel)

        self.layout_imagenes = QtWidgets.QGridLayout(self.ui.scrollAreaWidgetContents)
        self.layout_imagenes.setAlignment(QtCore.Qt.AlignTop)
        self.layout_imagenes.setSpacing(10)

        # Disable horizontal scrollbar on scroll area
        self.ui.scrollArea.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        # Ensure scrollAreaWidgetContents is wide enough for three images + spacing
        min_width = 3 * 120 + 4 * self.layout_imagenes.spacing()
        self.ui.scrollAreaWidgetContents.setMinimumWidth(min_width)

        self.actualizar_valor_kernel(self.ui.kernel_slider.value())

        # Async process for MPI
        self.proc = QtCore.QProcess(self)
        self.proc.setProcessChannelMode(QtCore.QProcess.MergedChannels)
        self.proc.readyRead.connect(self.handle_stdout)
        self.proc.finished.connect(self.handle_finished)

        self.total_images = 0

        # Timer for checking ./processed
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.check_progress_folder)

    def seleccionar_entrada(self):
        carpeta = QtWidgets.QFileDialog.getExistingDirectory(self, "Selecciona carpeta de entrada")
        if carpeta:
            self.ui.carpetaEntrada.setText(carpeta)

    def seleccionar_salida(self):
        carpeta = QtWidgets.QFileDialog.getExistingDirectory(self, "Selecciona carpeta de salida")
        if carpeta:
            self.ui.carpetaSalida.setText(carpeta)

    def actualizar_valor_kernel(self, valor):
        if valor % 2 == 0:
            valor += 1
        self.ui.label_6.setText(f"Kernel: {valor}")

    def mostrar_integrantes(self):
        QtWidgets.QMessageBox.information(self, "Equipo",
            "David Martínez Molina - A01735425\n"
            "Daniel Francisco Acosta Vázquez - A01736279\n"
            "Raúl Díaz Romero - A01735839\n"
            "Horacio Iann Toquiantzi Escarcega - A01734839"
        )

    def procesar(self):
        entrada = self.ui.carpetaEntrada.text()
        salida  = self.ui.carpetaSalida.text()
        kernel  = self.ui.kernel_slider.value() | 1  # ensure odd

        if not entrada or not salida:
            QtWidgets.QMessageBox.warning(self, "Advertencia",
                                          "Debes seleccionar ambas carpetas.")
            return

        # Clear previous output images
        if os.path.isdir(salida):
            for f in os.listdir(salida):
                if f.lower().endswith('.bmp'):
                    try:
                        os.remove(os.path.join(salida, f))
                    except Exception:
                        pass
        else:
            os.makedirs(salida, exist_ok=True)

        # Count .bmp files in input
        bmp_files = [f for f in os.listdir(entrada) if f.lower().endswith('.bmp')]
        total = len(bmp_files)
        if total == 0:
            QtWidgets.QMessageBox.information(self, "Sin imágenes",
                                              "No se encontraron imágenes .bmp.")
            return

        self.total_images = total
        # Setup progress
        self.ui.progressBar.setMaximum(self.total_images)
        self.ui.progressBar.setValue(0)
        self.ui.estimatedTime.setText(f"Imágenes procesadas: 0/{self.total_images}")

        # Start timer for dynamic progress update
        self.timer.start(500)  # every 0.5 seconds for smoother updates

        hostfile = "/mirror/machinefile"
        args = [
            "--mca", "btl_tcp_if_include", "enp0s8",
            "--mca", "oob_tcp_if_include", "enp0s8",
            "--mca", "plm_rsh_no_tree_spawn", "1",
            "--hostfile", hostfile,
            "./main.exe",
            str(kernel), entrada, salida, str(total)
        ]

        self.proc.start("mpirun", args)

    def handle_stdout(self):
        # Call folder check immediately on any stdout
        self.check_progress_folder()
        _ = self.proc.readAllStandardOutput()

    def handle_finished(self):
        # Ensure the last folder check
        self.check_progress_folder()
        self.timer.stop()

        self.ui.progressBar.setValue(self.total_images)
        self.ui.estimatedTime.setText(
            f"Imágenes procesadas: {self.total_images}/{self.total_images}"
        )

        log_path = os.path.join(os.getcwd(), "final_log.txt")
        if os.path.exists(log_path):
            with open(log_path, 'r', encoding='utf-8') as f:
                self.ui.txt_report.setPlainText(f.read())
        else:
            self.ui.txt_report.setPlainText("No se encontró final_log.txt")

        self.limpiar_imagenes()
        self.mostrar_imagenes_procesadas(self.ui.carpetaSalida.text())

    def check_progress_folder(self):
        output_dir = self.ui.carpetaSalida.text()
        if not os.path.isdir(output_dir):
            return

        processed_files = len([f for f in os.listdir(output_dir) if f.lower().endswith(".bmp")])
        # Each input image produces 6 outputs
        images_done = processed_files // 6
        # Clamp
        if images_done > self.total_images:
            images_done = self.total_images

        self.ui.progressBar.setValue(images_done)
        self.ui.estimatedTime.setText(f"Imágenes procesadas: {images_done}/{self.total_images}")

    def generar_txt(self):
        log_path = os.path.join(os.getcwd(), "final_log.txt")
        try:
            with open(log_path, "r", encoding="utf-8") as f:
                contenido = f.read()
            with open("reporte_detallado.txt", "w", encoding="utf-8") as f_out:
                f_out.write(contenido)
            QtWidgets.QMessageBox.information(self, "Reporte",
                                              "Reporte detallado guardado como 'reporte_detallado.txt'")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error",
                                           f"No se pudo guardar el reporte:\n{e}")

    def limpiar_imagenes(self):
        while self.layout_imagenes.count():
            item = self.layout_imagenes.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()

    def mostrar_imagenes_procesadas(self, ruta_salida):
        # Sort numerically by extracting leading digits
        def extract_num(name):
            m = re.match(r".*?(\d+)", name)
            return int(m.group(1)) if m else float('inf')

        bmp_files = sorted(
            [f for f in os.listdir(ruta_salida) if f.lower().endswith(".bmp")],
            key=extract_num
        )
        row = col = 0
        for nombre in bmp_files:
            ruta = os.path.join(ruta_salida, nombre)
            label_img = QtWidgets.QLabel()
            pixmap = QtGui.QPixmap(ruta).scaled(120, 120,
                                                QtCore.Qt.KeepAspectRatio,
                                                QtCore.Qt.SmoothTransformation)
            label_img.setPixmap(pixmap)
            label_img.setAlignment(QtCore.Qt.AlignCenter)

            label_nombre = QtWidgets.QLabel(nombre)
            label_nombre.setAlignment(QtCore.Qt.AlignCenter)
            label_nombre.setWordWrap(True)

            contenedor = QtWidgets.QVBoxLayout()
            contenedor.setAlignment(QtCore.Qt.AlignCenter)
            contenedor.addWidget(label_img)
            contenedor.addWidget(label_nombre)

            widget = QtWidgets.QWidget()
            widget.setLayout(contenedor)

            self.layout_imagenes.addWidget(widget, row, col)
            col += 1
            if col == 3:
                col = 0
                row += 1

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    ventana = MainWindow()
    ventana.show()
    sys.exit(app.exec_())
