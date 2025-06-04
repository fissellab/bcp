import json
from PIL import Image
import io

def encode_json(data) -> bytes:
    json_string = json.dumps(data)
    return json_string.encode('utf-8')

def encode_img(image: Image.Image) -> bytes:
    byte_stream = io.BytesIO()
    image.save(byte_stream, format="webp", quality=100)  # Adjust quality as needed
    compressed_bytes = byte_stream.getvalue()
    return compressed_bytes

def encode_msg(message: str) -> bytes:
    return message.encode('utf-8')