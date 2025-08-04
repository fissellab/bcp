#!/usr/bin/env python3
"""
Test script to debug 120kHz spectrum data reception and parsing
Run this on the CLIENT machine to test what data is being received
"""

import socket
import sys
import time

def test_120khz_spectrum_reception(server_ip="100.70.234.8", server_port=8081):
    """Test 120kHz spectrum data reception with detailed debugging"""
    
    print("=== 120kHz Spectrum Reception Test ===")
    print(f"Server: {server_ip}:{server_port}")
    print()
    
    # Create UDP socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(10.0)  # 10 second timeout
        print("✅ Socket created successfully")
    except Exception as e:
        print(f"❌ Failed to create socket: {e}")
        return
    
    try:
        # Send request for 120kHz spectrum
        request = "GET_SPECTRA_120KHZ"
        print(f"📤 Sending request: '{request}'")
        
        sock.sendto(request.encode('utf-8'), (server_ip, server_port))
        
        # Receive response
        print("⏳ Waiting for response...")
        response_data = sock.recv(32768)
        response = response_data.decode('utf-8')
        
        print(f"📥 Received {len(response_data)} bytes")
        print()
        
        # Show raw response (first 200 chars)
        print("=== RAW RESPONSE (first 200 chars) ===")
        print(repr(response[:200]))
        print()
        
        # Show full response if short, or truncated if long
        if len(response) <= 500:
            print("=== FULL RESPONSE ===")
            print(response)
        else:
            print("=== RESPONSE (truncated) ===")
            print(response[:250] + " ... " + response[-250:])
        print()
        
        # Parse the response
        if response.startswith("SPECTRA_120KHZ:"):
            print("✅ Response is 120kHz spectrum data")
            
            # Remove prefix
            content = response[15:]  # Remove "SPECTRA_120KHZ:"
            print(f"Content after prefix removal: {content[:100]}...")
            
            # Parse timestamp
            if content.startswith("timestamp:"):
                parts = content.split(',', 1)
                timestamp_part = parts[0]
                timestamp = float(timestamp_part.split(':')[1])
                metadata_and_data = parts[1] if len(parts) > 1 else ""
            else:
                parts = content.split(',', 1)
                timestamp = float(parts[0])
                metadata_and_data = parts[1] if len(parts) > 1 else ""
            
            print(f"📅 Timestamp: {timestamp}")
            
            # Find data section
            data_start = metadata_and_data.find('data:')
            if data_start == -1:
                print("❌ No 'data:' section found!")
                return
            
            # Parse metadata
            metadata_part = metadata_and_data[:data_start].rstrip(',')
            print(f"📋 Metadata: {metadata_part}")
            
            points = None
            freq_start = None
            freq_end = None
            baseline = None
            
            for item in metadata_part.split(','):
                if 'points:' in item:
                    points = int(item.split(':')[1])
                elif 'freq_start:' in item:
                    freq_start = float(item.split(':')[1])
                elif 'freq_end:' in item:
                    freq_end = float(item.split(':')[1])
                elif 'baseline:' in item:
                    baseline = float(item.split(':')[1])
            
            print(f"📊 Parsed metadata:")
            print(f"   Points: {points}")
            print(f"   Freq start: {freq_start} GHz")
            print(f"   Freq end: {freq_end} GHz")
            print(f"   Baseline: {baseline} dB")
            
            # Parse data
            data_str = metadata_and_data[data_start + 5:]  # Skip 'data:'
            print(f"📈 Data string length: {len(data_str)} chars")
            print(f"   First 50 chars: {data_str[:50]}")
            print(f"   Last 50 chars: {data_str[-50:]}")
            
            try:
                data = [float(x) for x in data_str.split(',') if x.strip()]
                print(f"✅ Successfully parsed {len(data)} data points")
                
                if data:
                    data_min, data_max = min(data), max(data)
                    data_mean = sum(data) / len(data)
                    
                    print(f"📈 Data statistics:")
                    print(f"   Min: {data_min:.6f} dB")
                    print(f"   Max: {data_max:.6f} dB")
                    print(f"   Mean: {data_mean:.6f} dB")
                    print(f"   Range: {data_max - data_min:.6f} dB")
                    
                    print(f"📋 Sample values:")
                    print(f"   [0]: {data[0]:.6f} dB")
                    if len(data) > 1:
                        print(f"   [mid]: {data[len(data)//2]:.6f} dB")
                        print(f"   [end]: {data[-1]:.6f} dB")
                    
                    # Expected values check
                    print()
                    print("=== EXPECTED vs RECEIVED ===")
                    print(f"Expected baseline: ~-44.6 dB")
                    print(f"Received baseline: {baseline:.6f} dB")
                    print(f"Baseline difference: {abs(-44.6 - baseline):.6f} dB")
                    
                    print(f"Expected data range: ~[-0.3, +10] dB (baseline-subtracted)")
                    print(f"Received data range: [{data_min:.6f}, {data_max:.6f}] dB")
                    
                    # Check if data looks reasonable
                    if -1.0 <= data_min <= 1.0 and 5.0 <= data_max <= 15.0:
                        print("✅ Data range looks reasonable for baseline-subtracted spectrum")
                    else:
                        print("⚠️  Data range seems unusual")
                        
                    if -46.0 <= baseline <= -42.0:
                        print("✅ Baseline value looks reasonable")
                    else:
                        print("⚠️  Baseline value seems unusual")
                
            except Exception as e:
                print(f"❌ Failed to parse data: {e}")
                print(f"   Problem area: {data_str[:100]}...")
        
        elif response.startswith("ERROR:"):
            print(f"❌ Server returned error: {response}")
        else:
            print(f"❌ Unknown response format: {response[:100]}...")
            
    except socket.timeout:
        print("❌ Request timed out - server may not be responding")
    except Exception as e:
        print(f"❌ Error during test: {e}")
    finally:
        sock.close()
        print()
        print("=== Test completed ===")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        server_ip = sys.argv[1]
    else:
        server_ip = "100.70.234.8"  # Default server IP
    
    if len(sys.argv) > 2:
        server_port = int(sys.argv[2])
    else:
        server_port = 8081  # Default port
    
    print(f"Usage: {sys.argv[0]} [server_ip] [server_port]")
    print(f"Using server: {server_ip}:{server_port}")
    print()
    
    test_120khz_spectrum_reception(server_ip, server_port) 