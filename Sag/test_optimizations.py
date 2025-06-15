#!/usr/bin/env python3
"""
Quick test to verify FPGA read optimizations
"""
import time
import subprocess
import os

def run_test(name, command, duration=120):
    """Run a test for specified duration and return average Hz"""
    print(f"\n=== {name} ===")
    print(f"Command: {command}")
    print(f"Duration: {duration} seconds")
    
    # Start the process
    process = subprocess.Popen(command, shell=True, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.STDOUT,
                              universal_newlines=True)
    
    start_time = time.time()
    rates = []
    
    try:
        while time.time() - start_time < duration:
            line = process.stdout.readline()
            if line:
                print(line.strip())
                # Look for performance lines
                if "Performance:" in line and "Hz" in line:
                    try:
                        # Extract Hz value
                        hz_part = line.split("=")[1].strip().split()[0]
                        rate = float(hz_part)
                        rates.append(rate)
                        print(f"  → Captured rate: {rate} Hz")
                    except:
                        pass
            
            # Check if process ended
            if process.poll() is not None:
                break
                
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    finally:
        process.terminate()
        process.wait()
    
    if rates:
        avg_rate = sum(rates) / len(rates)
        print(f"\n{name} Results:")
        print(f"  Average rate: {avg_rate:.2f} Hz")
        print(f"  Samples: {len(rates)}")
        return avg_rate
    else:
        print(f"\n{name}: No performance data captured")
        return 0

if __name__ == "__main__":
    # Test configurations
    base_cmd = "python3 rfsoc_spec_120khz.py 172.20.3.12 /home/mayukh/bcp/Sag/log/test.log cx"
    
    tests = [
        ("Baseline (Default)", f"{base_cmd} --timing"),
        ("Optimized I/O", f"{base_cmd} --flush-every 50 --sync-every 100 --timing"),
        ("Maximum Performance", f"{base_cmd} --flush-every 200 --sync-every 400 --timing"),
    ]
    
    results = {}
    
    for name, cmd in tests:
        rate = run_test(name, cmd, duration=90)  # 90 seconds per test
        results[name] = rate
    
    # Summary
    print("\n" + "="*50)
    print("PERFORMANCE COMPARISON SUMMARY")
    print("="*50)
    
    for name, rate in results.items():
        print(f"{name:25s}: {rate:6.2f} Hz")
    
    # Show improvements
    if results:
        baseline = results.get("Baseline (Default)", 0)
        if baseline > 0:
            print(f"\nIMPROVEMENTS vs Baseline:")
            for name, rate in results.items():
                if name != "Baseline (Default)" and rate > 0:
                    improvement = (rate / baseline - 1) * 100
                    print(f"{name:25s}: {improvement:+6.1f}%") 