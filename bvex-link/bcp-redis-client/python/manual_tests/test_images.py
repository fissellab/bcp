from PIL import Image, ImageDraw, ImageTk
import tkinter as tk
from bcp_redis_client.sample import set_sample_file_from_bytes
import redis
import io

def update_image(x, y):
    # Create a blank white image
    img = Image.new("RGB", (200, 200), "white")
    draw = ImageDraw.Draw(img)

    # Draw the circle at the current position
    radius = 20
    draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill="blue")

    return img

def animate():
    global x, y, dx, dy, tk_image, label, r

    # Update the position of the circle
    x += dx
    y += dy

    # Check for collisions with the edges and reverse direction if needed
    if x - radius <= 0 or x + radius >= 200:
        dx *= -1
    if y - radius <= 0 or y + radius >= 200:
        dy *= -1

    # Update the image
    img = update_image(x, y)
    
    #downlink image
    img_byte_arr = io.BytesIO()
    img.save(img_byte_arr, format='PNG')
    img_byte_arr = img_byte_arr.getvalue()
    set_sample_file_from_bytes(r, "bouncy-circle", img_byte_arr, "image.png")
    
    #test
    

    # Convert the PIL image to a format tkinter can use
    tk_image = ImageTk.PhotoImage(img)
    label.config(image=tk_image)
    label.image = tk_image

    # Schedule the next frame
    root.after(50, animate)
    
r = redis.Redis()

# Initialize tkinter
root = tk.Tk()
root.title("Bouncing Sphere Animation")

# Create a label to display the image
radius = 20
x, y = 100, 100  # Initial position of the circle
dx, dy = 5, 3    # Initial velocity of the circle
tk_image = None
label = tk.Label(root)
label.pack()

# Start the animation
animate()

# Run the tkinter main loop
root.mainloop()