import tkinter as tk
from PIL import Image, ImageTk


class ImageDrawer:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Live Image Viewer")

        self.label = tk.Label(root)
        self.label.pack()
        self.running = True
        # Store the current image to prevent garbage collection
        self._current_image = None

    def update_image(self, new_image: Image.Image):
        tk_image = ImageTk.PhotoImage(new_image)
        self.label.configure(image=tk_image)  # type: ignore
        # We need to keep a reference to prevent the image from being garbage collected
        # This is a known tkinter requirement
        self._current_image = tk_image  # type: ignore
