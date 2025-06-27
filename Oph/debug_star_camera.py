#!/usr/bin/env python3
"""
Debug script for star camera image processing
Run this to test image download and display independently
"""

import sys
import os
import logging
from PIL import Image
import io

# Add src to path
sys.path.append(os.path.join(os.path.dirname(__file__), 'src'))

from src.data.star_camera_client import StarCameraClient

def main():
    # Set up logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    
    logger = logging.getLogger(__name__)
    
    # Create client
    client = StarCameraClient()
    
    logger.info("Testing star camera connection and image download...")
    
    # Test server status
    status = client.get_status()
    if status and status.valid:
        logger.info(f"Server status: Active={status.is_active}, Queue={status.queue_size}, Bandwidth={status.bandwidth_kbps} kbps")
    else:
        logger.error("Failed to get server status")
        return
    
    # Test image download
    logger.info("Attempting to download latest image...")
    image = client.get_latest_image()
    
    if image and image.valid:
        logger.info(f"Image downloaded successfully:")
        logger.info(f"  Size: {image.width}x{image.height}")
        logger.info(f"  Data size: {len(image.image_data)} bytes")
        logger.info(f"  Quality: {image.compression_quality}")
        logger.info(f"  Stars: {image.blob_count}")
        
        # Save raw image data
        raw_filename = f"debug_image_raw_{image.timestamp}.jpg"
        with open(raw_filename, "wb") as f:
            f.write(image.image_data)
        logger.info(f"Raw image data saved to: {raw_filename}")
        
        # Test PIL processing
        try:
            pil_image = Image.open(io.BytesIO(image.image_data))
            logger.info(f"PIL image opened successfully:")
            logger.info(f"  Mode: {pil_image.mode}")
            logger.info(f"  Size: {pil_image.size}")
            logger.info(f"  Format: {pil_image.format}")
            
            # Save as PNG for verification
            png_filename = f"debug_image_processed_{image.timestamp}.png"
            pil_image.save(png_filename, "PNG")
            logger.info(f"Processed image saved to: {png_filename}")
            
            # Check if image has actual content
            import numpy as np
            np_array = np.array(pil_image)
            logger.info(f"Image statistics:")
            logger.info(f"  Shape: {np_array.shape}")
            logger.info(f"  Data type: {np_array.dtype}")
            logger.info(f"  Min value: {np_array.min()}")
            logger.info(f"  Max value: {np_array.max()}")
            logger.info(f"  Mean value: {np_array.mean():.2f}")
            
            if np_array.max() == np_array.min():
                logger.warning("Image appears to be uniform (all same value) - this could explain black display")
            else:
                logger.info("Image has varying pixel values - should display correctly")
            
        except Exception as e:
            logger.error(f"Error processing image with PIL: {e}")
    
    else:
        logger.error("Failed to download image")
        logger.info("Possible reasons:")
        logger.info("  - No images available on server")
        logger.info("  - Network connectivity issues")
        logger.info("  - Server not responding")

if __name__ == "__main__":
    main() 