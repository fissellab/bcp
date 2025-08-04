import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import os
import numpy as np
import paramiko
import re
import time

HOST = '100.70.234.8'
USERNAME = 'saggitarius'
PASSWORD = 'three_little_pigs!'

class GpsData:
    def __init__(self):
        self.valid = False
        self.year = 0
        self.month = 0
        self.day = 0
        self.hour = 0
        self.minute = 0
        self.second = 0
        self.latitude = 0.0
        self.longitude = 0.0
        self.altitude = 0.0  # Note: GPRMC doesn't have altitude information
        self.speed = 0.0
        self.heading = 0.0
        self.datetime = None
    
    def parse_gprmc(self, sentence):
        # Expected format: $GPRMC,time,status,lat,N/S,lon,E/W,speed,track,date,variation,E/W*checksum
        # Example: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
        parts = sentence.split(',')
        if len(parts) < 10:
            return False
        
        # Check if data is valid
        if parts[2] != 'A':
            return False
            
        # Parse time (HHMMSS.SSS)
        time_str = parts[1]
        if len(time_str) >= 6:
            self.hour = int(time_str[0:2])
            self.minute = int(time_str[2:4])
            self.second = int(time_str[4:6])
        
        # Parse latitude (DDMM.MMMM)
        if parts[3] and parts[4]:
            lat = float(parts[3])
            lat_degrees = int(lat / 100)
            lat_minutes = lat - (lat_degrees * 100)
            self.latitude = lat_degrees + (lat_minutes / 60)
            if parts[4] == 'S':
                self.latitude = -self.latitude
        
        # Parse longitude (DDDMM.MMMM)
        if parts[5] and parts[6]:
            lon = float(parts[5])
            lon_degrees = int(lon / 100)
            lon_minutes = lon - (lon_degrees * 100)
            self.longitude = lon_degrees + (lon_minutes / 60)
            if parts[6] == 'W':
                self.longitude = -self.longitude
        
        # Parse speed (in knots)
        if parts[7]:
            self.speed = float(parts[7])
        
        # Parse track/course over ground
        if parts[8]:
            self.heading = float(parts[8])
        
        # Parse date (DDMMYY)
        date_str = parts[9]
        if len(date_str) == 6:
            self.day = int(date_str[0:2])
            self.month = int(date_str[2:4])
            self.year = 2000 + int(date_str[4:6])  # Assuming 20xx years
            
        self.valid = True
        self.datetime = datetime(self.year, self.month, self.day, 
                                 self.hour, self.minute, self.second)
        return True
    
    def parse_gphdt(self, sentence):
        # Expected format: $GPHDT,heading,T*checksum
        # Example: $GPHDT,274.07,T*03
        parts = sentence.split(',')
        if len(parts) < 3:
            return False
            
        if parts[1]:
            self.heading = float(parts[1])
            return True
        return False

def load_and_parse_gps_files(ssh, folder):
    all_data = []
    print(f"Searching for GPS log files in folder: {folder}")
    stdin, stdout, stderr = ssh.exec_command(f'ls -1 {folder}/gps_log_*.bin')
    files = stdout.read().decode().splitlines()
    print(f"Found {len(files)} GPS log files")
    
    for file in files:
        print(f"Processing file: {file}")
        stdin, stdout, stderr = ssh.exec_command(f'cat {file}')
        binary_data = stdout.read()
        print(f"Read {len(binary_data)} bytes from file")
        
        # Convert binary data to text and split by lines
        text_data = binary_data.decode('utf-8', errors='replace')
        lines = text_data.splitlines()
        
        current_data = None
        
        for line in lines:
            if line.startswith('$GPRMC'):
                # Start a new data point with GPRMC data
                if current_data and current_data.valid:
                    all_data.append(current_data)
                    
                current_data = GpsData()
                try:
                    current_data.parse_gprmc(line)
                except Exception as e:
                    print(f"Error parsing GPRMC: {e}, line: {line}")
                    current_data = None
                
            elif line.startswith('$GPHDT') and current_data:
                # Add heading data to current point
                try:
                    current_data.parse_gphdt(line)
                except Exception as e:
                    print(f"Error parsing GPHDT: {e}, line: {line}")
        
        # Add the last data point if valid
        if current_data and current_data.valid:
            all_data.append(current_data)
    
    print(f"Total valid data points collected: {len(all_data)}")
    return all_data

def plot_data(data):
    if not data:
        print("No data to plot")
        return
        
    # Extract data for plotting
    times = [(d.hour * 60 + d.minute + d.second / 60) for d in data]  # minutes since midnight
    lats = [d.latitude for d in data]
    lons = [d.longitude for d in data]
    headings = [d.heading for d in data]
    speeds = [d.speed for d in data]

    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 15))
    
    ax1.plot(times, lats)
    ax1.set_xlabel('Time (minutes since midnight)')
    ax1.set_ylabel('Latitude')
    ax1.set_title('Latitude over Time')
    ax1.ticklabel_format(useOffset=False, style='plain')

    ax2.plot(times, lons)
    ax2.set_xlabel('Time (minutes since midnight)')
    ax2.set_ylabel('Longitude')
    ax2.set_title('Longitude over Time')
    ax2.ticklabel_format(useOffset=False, style='plain')

    # Note: GPRMC doesn't provide altitude, so we'll plot speed instead
    ax3.plot(times, speeds)
    ax3.set_xlabel('Time (minutes since midnight)')
    ax3.set_ylabel('Speed (knots)')
    ax3.set_title('Speed over Time')

    ax4.plot(times, headings)
    ax4.set_xlabel('Time (minutes since midnight)')
    ax4.set_ylabel('Heading (degrees)')
    ax4.set_title('Heading over Time')

    plt.tight_layout()
    plt.show()
    

def calculate_statistics(data):
    if not data:
        return {
            'total_distance': 0,
            'max_speed': 0,
            'avg_speed': 0,
            'avg_heading': None,
            'duration': '00:00:00'
        }

    total_distance = 0
    speeds = [d.speed for d in data]
    headings = [d.heading for d in data]
    
    # Calculate distance using Haversine formula
    for i in range(1, len(data)):
        prev, curr = data[i-1], data[i]
        lat1, lon1 = np.radians(prev.latitude), np.radians(prev.longitude)
        lat2, lon2 = np.radians(curr.latitude), np.radians(curr.longitude)
        
        # Haversine formula for distance
        dlat = lat2 - lat1
        dlon = lon2 - lon1
        a = np.sin(dlat/2)**2 + np.cos(lat1) * np.cos(lat2) * np.sin(dlon/2)**2
        c = 2 * np.arcsin(np.sqrt(a))
        r = 6371  # Radius of earth in kilometers
        distance = c * r
        total_distance += distance
    
    # Calculate trip duration
    if len(data) > 1:
        first_time = data[0].datetime
        last_time = data[-1].datetime
        
        # Handle day change
        if last_time < first_time:
            last_time += timedelta(days=1)
            
        duration = last_time - first_time
    else:
        duration = timedelta(0)

    return {
        'total_distance': total_distance,
        'max_speed': max(speeds) if speeds else 0,
        'avg_speed': sum(speeds) / len(speeds) if speeds else 0,
        'avg_heading': sum(headings) / len(headings) if headings else None,
        'duration': str(duration)
    }

def connect_to_remote():
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        ssh.connect(HOST, username=USERNAME, password=PASSWORD)
        return ssh
    except Exception as e:
        print(f"Failed to connect: {e}")
        return None

def get_remote_folders(ssh):
    stdin, stdout, stderr = ssh.exec_command('ls -d /media/saggitarius/T7/GPS_data/*/')
    folders = stdout.read().decode().splitlines()
    return [folder.strip() for folder in folders]

def select_remote_folder(folders):
    print("Available GPS data folders:")
    for i, folder in enumerate(folders, 1):
        print(f"{i}. {os.path.basename(folder.rstrip('/'))}")
    
    while True:
        try:
            choice = int(input("Enter the number of the folder you want to analyze (or 0 to exit): "))
            if choice == 0:
                return None
            if 1 <= choice <= len(folders):
                return folders[choice - 1]
            else:
                print("Invalid choice. Please try again.")
        except ValueError:
            print("Invalid input. Please enter a number.")

def main():
    ssh = connect_to_remote()
    if not ssh:
        return

    remote_folders = get_remote_folders(ssh)
    if not remote_folders:
        print("No GPS data folders found.")
        ssh.close()
        return
        
    selected_remote_folder = select_remote_folder(remote_folders)
    if not selected_remote_folder:
        ssh.close()
        return

    print(f"Selected folder: {selected_remote_folder}")
    start_time = time.time()
    data = load_and_parse_gps_files(ssh, selected_remote_folder)
    end_time = time.time()
    ssh.close()

    if not data:
        print("No valid GPS data found in the folder.")
        return

    print(f"Successfully parsed {len(data)} data points in {end_time - start_time:.2f} seconds.")

    plot_data(data)
    
    stats = calculate_statistics(data)
    print("\nStatistics:")
    print(f"Total distance traveled: {stats['total_distance']:.2f} km")
    print(f"Maximum speed: {stats['max_speed']:.2f} knots")
    print(f"Average speed: {stats['avg_speed']:.2f} knots")
    if stats['avg_heading'] is not None:
        print(f"Average heading: {stats['avg_heading']:.2f}°")
    print(f"Trip duration: {stats['duration']}")
    
    print(f"\nProcessed {len(data)} data points")

    # Extract and return all heading values
    heading_values = [d.heading for d in data]
    return heading_values



if __name__ == "__main__":
    main()
    headings = main()