import sys
import subprocess
import os
import time
from PyQt5 import QtWidgets, QtGui, QtCore
from gui5 import Ui_MainWindow

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)

        self.ui.seleccionarCarpetaEntrada.clicked.connect(self.seleccionar_entrada)
        self.ui.seleccionarCarpetaSalida.clicked.connect(self.seleccionar_salida)
        self.ui.processImages.clicked.connect(self.procesar)
        self.ui.generateReport.clicked.connect(self.generar_txt)
        self.ui.actionMostrarIntegrantes.triggered.connect(self.mostrar_integrantes)
        self.ui.kernel_slider.valueChanged.connect(self.actualizar_valor_kernel)

        self.layout_imagenes = QtWidgets.QGridLayout(self.ui.scrollAreaWidgetContents)
        self.layout_imagenes.setAlignment(QtCore.Qt.AlignTop)
        self.layout_imagenes.setSpacing(10)

        self.actualizar_valor_kernel(self.ui.kernel_slider.value())

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
        self.ui.progressBar.setValue(0)
        self.limpiar_imagenes()
        self.ui.estimatedTime.setText("Estimando...")

        entrada = self.ui.carpetaEntrada.text()
        salida = self.ui.carpetaSalida.text()
        kernel = self.ui.kernel_slider.value()
        if kernel % 2 == 0:
            kernel += 1

        if not entrada or not salida:
            QtWidgets.QMessageBox.warning(self, "Advertencia", "Debes seleccionar ambas carpetas.")
            return

        if not os.path.exists(salida):
            os.makedirs(salida)

        imagenes = sorted([f for f in os.listdir(entrada) if f.lower().endswith(".bmp")])
        total = len(imagenes)
        if total == 0:
            QtWidgets.QMessageBox.information(self, "Sin imágenes", "No se encontraron imágenes .bmp.")
            return

        self.ui.progressBar.setMaximum(total)

        # Limpiar log antes de ejecutar
        with open("log.txt", "w", encoding="utf-8") as f:
            f.truncate(0)

        inicio = time.time()

        comando = ["main.exe", str(kernel), entrada, salida, str(total)]
        try:
            subprocess.run(comando, check=True)
            self.ui.progressBar.setValue(total)
        except subprocess.CalledProcessError as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"Error al procesar imágenes:\n{e}")
            return

        fin = time.time()
        segundos = int(fin - inicio)
        minutos = segundos // 60
        self.ui.estimatedTime.setText(f"Tiempo total: {minutos}m {segundos % 60}s")

        try:
            with open("log.txt", "r", encoding="utf-8") as f:
                self.ui.txt_report.setPlainText(f.read())
        except Exception as e:
            self.ui.txt_report.setPlainText(f"Error al leer log.txt:\n{e}")

        self.mostrar_imagenes_procesadas(salida)

    def generar_txt(self):
        try:
            with open("log.txt", "r", encoding="utf-8") as f:
                contenido = f.read()
            with open("reporte_detallado.txt", "w", encoding="utf-8") as f_out:
                f_out.write(contenido)
            QtWidgets.QMessageBox.information(self, "Reporte", "Reporte detallado guardado como 'reporte_detallado.txt'")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", f"No se pudo guardar el reporte:\n{e}")

    def limpiar_imagenes(self):
        while self.layout_imagenes.count():
            item = self.layout_imagenes.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()

    def mostrar_imagenes_procesadas(self, ruta_salida):
        bmp_files = sorted(f for f in os.listdir(ruta_salida) if f.lower().endswith(".bmp"))
        row = 0
        col = 0
        for nombre in bmp_files:
            ruta = os.path.join(ruta_salida, nombre)
            label_img = QtWidgets.QLabel()
            pixmap = QtGui.QPixmap(ruta).scaled(120, 120, QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation)
            label_img.setPixmap(pixmap)
            label_img.setAlignment(QtCore.Qt.AlignCenter)
            label_nombre = QtWidgets.QLabel(nombre)
            label_nombre.setAlignment(QtCore.Qt.AlignCenter)
            label_nombre.setWordWrap(True)
            contenedor = QtWidgets.QVBoxLayout()
            contenedor.setAlignment(QtCore.Qt.AlignCenter)
            widget = QtWidgets.QWidget()
            contenedor.addWidget(label_img)
            contenedor.addWidget(label_nombre)
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
