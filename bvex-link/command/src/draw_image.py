import tkinter as tk
from PIL import Image, ImageTk
import asyncio
from typing import Optional

no_sample_img = ImageTk.PhotoImage(Image.open("./assets/no_sample.png"))


async def open_window() -> "ImageWindow":
    """Create and return a new image window."""
    window = ImageWindow()
    return window


class ImageWindow:
    def __init__(self):
        

    def update(self, image: ImageTk.PhotoImage):
        """Update the window with a new image."""
        # Convert PIL image to tkinter PhotoImage
        self.label.configure(image=image)
        self.label.image = image  # Keep a reference to avoid garbage collection
        self.root.update()

    def close(self):
        """Close the window."""
        


class ImageDrawer:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Image Viewer")
        self.image: Optional[Image.Image] = None
        self.window: Optional[ImageWindow] = None

    async def __aenter__(self):
        """Initialize the window when entering the context."""
        self.window = await open_window()
        return self

    async def draw(self, image: Image.Image):
        """Draw the given image in the window."""
        self.image = image
        if self.window:
            self.window.update(image)
        else:
            raise RuntimeError(
                "Window not initialized. Use 'async with' context manager."
            )

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Clean up resources when exiting the context."""
        self.root.destroy()
