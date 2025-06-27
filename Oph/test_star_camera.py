#!/usr/bin/env python3
"""
Test script for Star Camera Client
Tests basic connectivity and protocol functionality
"""

import sys
import time
from src.data.star_camera_client import StarCameraClient

def test_star_camera_client():
    """Test the star camera client functionality"""
    print("Star Camera Client Test")
    print("=" * 40)
    
    # Create client
    client = StarCameraClient()
    print(f"Server: {client.host}:{client.port}")
    print(f"Timeout: {client.timeout}s")
    print()
    
    # Test 1: Get server status
    print("Test 1: Getting server status...")
    status = client.get_server_status()
    
    if status and status.valid:
        print("  ✓ Server is responding!")
        print(f"  Queue size: {status.queue_size} images")
        print(f"  Bandwidth usage: {status.bandwidth_usage_kbps} kbps")
        print(f"  Current transmission: {'ACTIVE' if status.current_transmission_active else 'IDLE'}")
    else:
        print("  ✗ No response from server (timeout or error)")
        print("  Make sure the server is running and accessible")
        return False
    
    print()
    
    # Test 2: Request latest image
    print("Test 2: Requesting latest image...")
    print("  This may take some time due to bandwidth limits...")
    
    start_time = time.time()
    image = client.get_latest_image()
    elapsed_time = time.time() - start_time
    
    if image and image.valid and len(image.image_data) > 0:
        print("  ✓ Image received successfully!")
        print(f"  Image timestamp: {image.timestamp}")
        print(f"  Image size: {image.total_size} bytes (compressed)")
        print(f"  Compression quality: {image.compression_quality}")
        print(f"  Stars detected: {image.blob_count}")
        print(f"  Image dimensions: {image.width}x{image.height}")
        print(f"  Download time: {elapsed_time:.1f} seconds")
        
        # Save the image
        filename = f"test_image_{image.timestamp}.jpg"
        try:
            with open(filename, 'wb') as f:
                f.write(image.image_data)
            print(f"  ✓ Image saved as: {filename}")
        except Exception as e:
            print(f"  ✗ Error saving image: {e}")
            
    else:
        print("  ✗ Failed to receive image or no image available")
        if image:
            print(f"  Image valid: {image.valid}")
            print(f"  Data length: {len(image.image_data) if image.image_data else 0}")
    
    print()
    print("Test completed!")
    return True

if __name__ == "__main__":
    try:
        success = test_star_camera_client()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nTest failed with error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1) 