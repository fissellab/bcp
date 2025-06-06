#!/usr/bin/env python3
"""
Test UDP client for spectrometer server
This script tests the UDP protocol by sending requests to the server
"""

import socket
import time
import sys

def test_udp_request(server_ip, server_port, request, description):
    """Send a UDP request and print the response"""
    print(f"\n🔄 Testing: {description}")
    print(f"   Request: '{request}'")
    
    try:
        # Create UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5.0)  # 5 second timeout
        
        # Send request
        message = request.encode('utf-8')
        sock.sendto(message, (server_ip, server_port))
        
        # Receive response
        response, addr = sock.recvfrom(32768)  # Large buffer for spectrum data
        response_str = response.decode('utf-8')
        
        # Parse response
        if response_str.startswith("SPECTRA_STD:"):
            parts = response_str.split(',')
            print(f"✅ Standard Spectrum Response:")
            print(f"   Timestamp: {parts[0].split(':')[1]}")
            print(f"   Points: {parts[1].split(':')[1]}")
            print(f"   Data size: {len(response)} bytes")
            
        elif response_str.startswith("SPECTRA_120KHZ:"):
            parts = response_str.split(',')
            print(f"✅ 120kHz Spectrum Response:")
            print(f"   Timestamp: {parts[0].split(':')[1]}")
            print(f"   Points: {parts[1].split(':')[1]}")
            print(f"   Freq start: {parts[2].split(':')[1]} GHz")
            print(f"   Freq end: {parts[3].split(':')[1]} GHz")
            print(f"   Baseline: {parts[4].split(':')[1]} dB")
            print(f"   Data size: {len(response)} bytes")
            
        elif response_str.startswith("ERROR:"):
            print(f"⚠️  Server Error: {response_str}")
            
        else:
            print(f"❓ Unknown Response: {response_str[:100]}...")
        
        sock.close()
        
    except socket.timeout:
        print(f"❌ Timeout - no response from server")
        sock.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")
        if 'sock' in locals():
            sock.close()

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 test_udp_client.py <server_ip> <server_port>")
        print("Example: python3 test_udp_client.py 127.0.0.1 8081")
        sys.exit(1)
    
    server_ip = sys.argv[1]
    server_port = int(sys.argv[2])
    
    print("Spectrometer Server UDP Client Test")
    print("=" * 50)
    print(f"Server: {server_ip}:{server_port}")
    
    # Test cases
    test_cases = [
        ("GET_SPECTRA", "Standard Spectrum Request"),
        ("GET_SPECTRA_120KHZ", "120kHz High-Resolution Spectrum Request"),
        ("INVALID_REQUEST", "Invalid Request (should return error)"),
        ("GET_SPECTRA", "Second Standard Request (rate limiting test)"),
    ]
    
    for request, description in test_cases:
        test_udp_request(server_ip, server_port, request, description)
        time.sleep(0.5)  # Small delay between requests
    
    print(f"\n✅ Test completed!")

if __name__ == "__main__":
    main() 