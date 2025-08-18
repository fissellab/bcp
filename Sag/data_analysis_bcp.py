import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import os
import re
import struct
import time
import posixpath
import stat
import paramiko
import getpass

_TS_SCALE = {'s': 1.0, 'ms': 1e-3, 'us': 1e-6, 'ns': 1e-9}

class RemoteFS:
    """
    Minimal POSIX-like filesystem wrapper over Paramiko SFTP.
    """
    def __init__(self, host=None, username=None, password=None, port=22):
        self.host = host or os.getenv("SAG_SSH_HOST", "localhost")
        self.username = username or os.getenv("SAG_SSH_USER", "saggitarius") 
        self.password = password or os.getenv("SAG_SSH_PASS", None)
        self.port = port
        # optional key auth
        self.key_filename = os.getenv("SAG_SSH_KEY", None)  # e.g. ~/.ssh/id_ed25519
        self.passphrase = os.getenv("SAG_SSH_PASSPHRASE", None)
        self._ssh = None
        self._sftp = None

    @property
    def sftp(self):
        return self._sftp

    def connect(self, timeout=30):
        self._ssh = paramiko.SSHClient()
        self._ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        # Resolve key path (if provided) and ignore when missing
        keyfile = self.key_filename
        if keyfile:
            keyfile = os.path.expanduser(keyfile)
            if not os.path.exists(keyfile):
                print(f"Info: SSH key not found at {keyfile}, falling back to password/agent.")
                keyfile = None

        def _do_connect(pwd):
            self._ssh.connect(
                self.host,
                port=self.port,
                username=self.username,
                password=pwd,
                key_filename=keyfile,
                look_for_keys=True,
                allow_agent=True,
                timeout=timeout,
                auth_timeout=timeout,
                banner_timeout=timeout,
                compress=True,
            )

        try:
            # Try with provided password (or none), agent, and optional keyfile
            _do_connect(self.password)
        except FileNotFoundError:
            # Retry without keyfile if Paramiko still tried to open it
            keyfile = None
            _do_connect(self.password)
        except paramiko.AuthenticationException:
            # Prompt for password if not provided/incorrect
            if not self.password:
                self.password = getpass.getpass(f"Password for {self.username}@{self.host}: ")
            _do_connect(self.password)

        self._sftp = self._ssh.open_sftp()
        print(f"✓ SSH connected to {self.username}@{self.host}")

    def close(self):
        try:
            if self._sftp:
                self._sftp.close()
        finally:
            if self._ssh:
                self._ssh.close()
        print("SSH connection closed")

    def join(self, *parts):
        return posixpath.join(*parts)

    def listdir(self, path):
        return self._sftp.listdir(path)

    def isdir(self, path):
        try:
            st = self._sftp.stat(path)
            return stat.S_ISDIR(st.st_mode)
        except Exception:
            return False

    def open(self, path, mode='rb'):
        # mode should be text/binary read only here; Paramiko supports 'rb', 'r'
        return self._sftp.open(path, mode)

    def getsize(self, path):
        return self._sftp.stat(path).st_size

def _extract_ts(buf: memoryview, off: int, ts_type: str, unit: str):
    # ts_type: 'd' (double seconds), 'Q' (uint64 ticks), 'I' (uint32 ticks), 'II' (sec + nsec)
    if ts_type == 'd':
        val = struct.unpack_from('<d', buf, off)[0]
        return float(val)
    elif ts_type == 'Q':
        val = struct.unpack_from('<Q', buf, off)[0]
        return float(val) * _TS_SCALE[unit]
    elif ts_type == 'I':
        val = struct.unpack_from('<I', buf, off)[0]
        return float(val) * _TS_SCALE[unit]
    elif ts_type == 'II':
        sec = struct.unpack_from('<I', buf, off)[0]
        nsec = struct.unpack_from('<I', buf, off + 4)[0]
        return float(sec) + float(nsec) * 1e-9
    else:
        return float('nan')

def _detect_record_format_remote(fs: RemoteFS, remote_path, candidates, probe_records=1000):
    """
    Detect the record format using remote SFTP file.
    """
    try:
        file_size = fs.getsize(remote_path)
    except Exception:
        # fallback
        return candidates[0]

    viable = [c for c in candidates if (c.get('size') and file_size % c['size'] == 0 and (file_size // c['size']) >= 2)]
    if not viable:
        return candidates[0]

    now_ts = time.time()
    low_ts = now_ts - 10 * 365 * 24 * 3600.0
    high_ts = now_ts + 10 * 365 * 24 * 3600.0

    best = None
    best_score = None

    for c in viable:
        try:
            rec_size = c['size']
            ts_type, unit = c.get('ts', ('d', 's'))
            nrec = min(max(8, probe_records), file_size // rec_size)
            with fs.open(remote_path, 'rb') as f:
                buf = f.read(nrec * rec_size)
            mv = memoryview(buf)

            ts = []
            bad_ii = 0
            for i in range(nrec):
                off = i * rec_size
                if ts_type == 'II':
                    sec = struct.unpack_from('<I', mv, off)[0]
                    nsec = struct.unpack_from('<I', mv, off + 4)[0]
                    if not (low_ts <= float(sec) <= high_ts) or (nsec >= 1_000_000_000):
                        bad_ii += 1
                    t = float(sec) + float(nsec) * 1e-9
                elif ts_type == 'Q':
                    t = float(struct.unpack_from('<Q', mv, off)[0]) * _TS_SCALE.get(unit, 1.0)
                elif ts_type == 'I':
                    t = float(struct.unpack_from('<I', mv, off)[0]) * _TS_SCALE.get(unit, 1.0)
                else:  # 'd'
                    t = float(struct.unpack_from('<d', mv, off)[0])
                ts.append(t)

            ts = np.asarray(ts, dtype=float)
            diffs = np.diff(ts)
            diffs = diffs[np.isfinite(diffs)]
            if diffs.size == 0:
                continue

            med = float(np.median(diffs))
            neg = int(np.sum(diffs <= 0))
            plausible_dt = (1e-6 <= med <= 2e-2)

            ts_min = float(np.nanmin(ts))
            ts_max = float(np.nanmax(ts))
            epoch_ok = (low_ts <= ts_min <= high_ts and low_ts <= ts_max <= high_ts)

            invalid_pen = 0 if plausible_dt else 2
            epoch_pen = 0 if epoch_ok else 1
            ii_pen = bad_ii
            ts_priority = {'d': 0, 'Q': 1, 'II': 2, 'I': 3}.get(ts_type, 4)
            with np.errstate(all='ignore'):
                jitter = float(np.std(diffs))

            score = (invalid_pen, epoch_pen, ii_pen, ts_priority, neg, jitter, rec_size)
            if best_score is None or score < best_score:
                best = c
                best_score = score
        except Exception:
            continue

    return best or viable[0]

def _combine_ii_timestamp(rows):
    """
    Convert rows that start with (uint32 sec, uint32 nsec, ...) into
    (float seconds, ...) where seconds = sec + nsec*1e-9.
    """
    out = []
    for r in rows:
        if len(r) < 3:
            continue
        sec, nsec = r[0], r[1]
        ts = float(sec) + float(nsec) * 1e-9
        out.append((ts,) + tuple(r[2:]))
    return out

def _convert_rows_timestamp(rows, ts):
    """
    Normalize first field to float seconds for non-double timestamp formats.
    ts is a tuple like ('d','s'), ('Q','ns'), ('II','ns'), ('I','ms'), etc.
    """
    if not rows:
        return rows
    ts_type, unit = ts
    if ts_type == 'd':
        return rows
    if ts_type == 'II':
        return _combine_ii_timestamp(rows)
    if ts_type in ('Q', 'I'):
        scale = _TS_SCALE.get(unit, 1.0)
        out = []
        for r in rows:
            if not r:
                continue
            t = float(r[0]) * scale
            out.append((t,) + tuple(r[1:]))
        return out
    return rows

def _read_binary_records_remote(fs: RemoteFS, remote_path, fmt):
    """
    Read the entire remote binary file as fixed-size records using struct format fmt.
    Returns a list of tuples (one per record).
    """
    if not fmt.startswith(('<', '>', '!', '@', '=')):
        fmt = '<' + fmt
    rec_size = struct.calcsize(fmt)
    if rec_size <= 0:
        return []

    with fs.open(remote_path, 'rb') as f:
        data = f.read()

    rem = len(data) % rec_size
    if rem:
        data = data[:-rem]

    return list(struct.iter_unpack(fmt, data))

# New helpers to resolve sensor folder names
def _first_existing_dir(fs: RemoteFS, base: str, candidates):
    for name in candidates:
        p = fs.join(base, name)
        if fs.isdir(p):
            return p
    return None

def _dir_exists(fs: RemoteFS, base: str, name: str):
    return fs.isdir(fs.join(base, name))

def load_and_process_accelerometer_files_remote(fs: RemoteFS, folder_path):
    """
    Remote version: Load and process all .bin files for three accelerometers via SFTP.
    Resolves folders named either accelerometer_{i} or accl{i}.
    """
    all_data = {1: [], 2: [], 3: []}

    for accel_id in [1, 2, 3]:
        accel_folder = _first_existing_dir(
            fs, folder_path, [f'accelerometer_{accel_id}', f'accl{accel_id}']
        )
        if not accel_folder:
            print(f"Warning: Accelerometer {accel_id} folder not found (looked for accelerometer_{accel_id}/accl{accel_id})")
            continue

        try:
            bin_files = sorted([fn for fn in fs.listdir(accel_folder) if fn.lower().endswith('.bin')])
        except Exception as e:
            print(f"Warning: Could not list {accel_folder}: {e}")
            continue

        if not bin_files:
            print(f"Warning: No .bin files found in {accel_folder}")
            continue

        for filename in bin_files:
            remote_file = fs.join(accel_folder, filename)
            # BCP format: double timestamp + 3 floats (x,y,z) = 20 bytes
            candidates = [
                {'name': 'BCP_accel', 'size': 20, 'fmt': '<dfff', 'ts': ('d', 's')},
                {'name': 'dfff_legacy', 'size': 20, 'fmt': '<dfff', 'ts': ('d', 's')},
                {'name': 'dddd_legacy', 'size': 32, 'fmt': '<dddd', 'ts': ('d', 's')},
            ]
            chosen = _detect_record_format_remote(fs, remote_file, candidates)
            rows = _read_binary_records_remote(fs, remote_file, chosen['fmt'])
            all_data[accel_id].extend(rows)
            print(f"Loaded {len(rows):,} accel{accel_id} records from {filename} using {chosen['name']} ({chosen['size']}B)")

    dfs = {}
    for accel_id in [1, 2, 3]:
        if all_data[accel_id]:
            df = pd.DataFrame(all_data[accel_id], columns=['Timestamp', 'X (m/s^2)', 'Y (m/s^2)', 'Z (m/s^2)'])
            
            # Convert to relative timestamps to handle incorrect absolute timestamps
            df = df.sort_values('Timestamp')
            start_timestamp = df['Timestamp'].iloc[0]
            df['Time_elapsed'] = df['Timestamp'] - start_timestamp
            
            # Create a datetime index based on relative time for compatibility
            df['Datetime'] = pd.to_datetime(df['Time_elapsed'], unit='s', origin='2025-01-01')
            df.set_index('Datetime', inplace=True)
            df = df.dropna()
            
            dfs[accel_id] = df if not df.empty else None
        else:
            print(f"Warning: No data found for accelerometer {accel_id}")
            dfs[accel_id] = None

    return dfs.get(1), dfs.get(2), dfs.get(3)

def load_and_process_spi_gyroscope_files_remote(fs: RemoteFS, folder_path):
    """
    Remote version: Load SPI gyro .bin files. Resolves 'spi_gyroscope' or 'gyro1'.
    """
    gyro_folder = _first_existing_dir(fs, folder_path, ['spi_gyroscope', 'gyro1'])
    if not gyro_folder:
        print(f"Warning: SPI Gyroscope folder not found (looked for spi_gyroscope/gyro1)")
        return None

    try:
        bin_files = sorted([fn for fn in fs.listdir(gyro_folder) if fn.lower().endswith('.bin')])
    except Exception as e:
        print(f"Warning: Could not list {gyro_folder}: {e}")
        return None

    if not bin_files:
        print("Warning: No .bin files found for SPI gyroscope")
        return None

    all_rows = []
    for filename in bin_files:
        remote_file = fs.join(gyro_folder, filename)
        # BCP format: double timestamp + 1 float (angular rate) = 12 bytes
        candidates = [
            {'name': 'BCP_spi_gyro', 'size': 12, 'fmt': '<df', 'ts': ('d', 's')},
            {'name': 'df_legacy', 'size': 12, 'fmt': '<df', 'ts': ('d', 's')},
            {'name': 'dd_legacy', 'size': 16, 'fmt': '<dd', 'ts': ('d', 's')},
        ]
        chosen = _detect_record_format_remote(fs, remote_file, candidates)
        rows = _read_binary_records_remote(fs, remote_file, chosen['fmt'])
        all_rows.extend(rows)
        print(f"Loaded {len(rows):,} SPI gyro records from {filename} using {chosen['name']} ({chosen['size']}B)")

    if all_rows:
        df = pd.DataFrame(all_rows, columns=['Timestamp', 'Angular_Rate (deg/s)'])
        
        # Convert to relative timestamps to handle incorrect absolute timestamps
        df = df.sort_values('Timestamp')
        start_timestamp = df['Timestamp'].iloc[0]
        df['Time_elapsed'] = df['Timestamp'] - start_timestamp
        
        # Create a datetime index based on relative time for compatibility
        df['Datetime'] = pd.to_datetime(df['Time_elapsed'], unit='s', origin='2025-01-01')
        df.set_index('Datetime', inplace=True)
        df = df.dropna()
        
        return df if not df.empty else None
    return None

def load_and_process_i2c_gyroscope_files_remote(fs: RemoteFS, folder_path):
    """
    Remote version: Load I2C gyro .bin files. Resolves 'i2c_gyroscope' or 'gyro_i2c1'.
    """
    gyro_folder = _first_existing_dir(fs, folder_path, ['i2c_gyroscope', 'gyro_i2c1'])
    if not gyro_folder:
        print(f"Warning: I2C Gyroscope folder not found (looked for i2c_gyroscope/gyro_i2c1)")
        return None

    try:
        bin_files = sorted([fn for fn in fs.listdir(gyro_folder) if fn.lower().endswith('.bin')])
    except Exception as e:
        print(f"Warning: Could not list {gyro_folder}: {e}")
        return None

    if not bin_files:
        print("Warning: No .bin files found for I2C gyroscope")
        return None

    all_rows = []
    for filename in bin_files:
        remote_file = fs.join(gyro_folder, filename)
        # BCP format: double timestamp + 4 floats (x,y,z,temperature) = 24 bytes
        candidates = [
            {'name': 'BCP_i2c_gyro', 'size': 24, 'fmt': '<dffff', 'ts': ('d', 's')},
            {'name': 'dffff_legacy', 'size': 24, 'fmt': '<dffff', 'ts': ('d', 's')},
            {'name': 'ddddd_legacy', 'size': 40, 'fmt': '<ddddd', 'ts': ('d', 's')},
        ]
        chosen = _detect_record_format_remote(fs, remote_file, candidates)
        rows = _read_binary_records_remote(fs, remote_file, chosen['fmt'])
        all_rows.extend(rows)
        print(f"Loaded {len(rows):,} I2C gyro records from {filename} using {chosen['name']} ({chosen['size']}B)")

    if all_rows:
        df = pd.DataFrame(all_rows, columns=['Timestamp', 'X (deg/s)', 'Y (deg/s)', 'Z (deg/s)', 'Temperature (°C)'])
        
        # Convert to relative timestamps to handle incorrect absolute timestamps
        df = df.sort_values('Timestamp')
        start_timestamp = df['Timestamp'].iloc[0]
        df['Time_elapsed'] = df['Timestamp'] - start_timestamp
        
        # Create a datetime index based on relative time for compatibility
        df['Datetime'] = pd.to_datetime(df['Time_elapsed'], unit='s', origin='2025-01-01')
        df.set_index('Datetime', inplace=True)
        df = df.dropna()
        
        return df if not df.empty else None
    return None

def select_remote_folder(fs: RemoteFS, base_path="/media/saggitarius/T7/position_tracking_data"):
    """
    Prompt user to select a dataset folder on the remote host.
    Accepts either accelerometer_*/spi_gyroscope/i2c_gyroscope or accl*/gyro1/gyro_i2c1.
    """
    try:
        candidates = sorted([d for d in fs.listdir(base_path) if fs.isdir(fs.join(base_path, d))])
    except Exception as e:
        print(f"Could not list base path {base_path}: {e}")
        candidates = []

    valid = []
    for d in candidates:
        path = fs.join(base_path, d)
        accel_set1 = all(_dir_exists(fs, path, f'accelerometer_{i}') for i in (1, 2, 3))
        accel_set2 = all(_dir_exists(fs, path, f'accl{i}') for i in (1, 2, 3))
        gyro_spi_ok = _dir_exists(fs, path, 'spi_gyroscope') or _dir_exists(fs, path, 'gyro1')
        gyro_i2c_ok = _dir_exists(fs, path, 'i2c_gyroscope') or _dir_exists(fs, path, 'gyro_i2c1')
        sub_ok = (accel_set1 or accel_set2) and gyro_spi_ok and gyro_i2c_ok
        if sub_ok:
            valid.append(d)

    print("Available remote datasets under", base_path)
    for i, d in enumerate(valid, 1):
        print(f"{i}. {d}")
    print(f"{len(valid) + 1}. Enter custom remote path")

    while True:
        try:
            choice = int(input("Enter the number of your choice: "))
            if 1 <= choice <= len(valid):
                return fs.join(base_path, valid[choice - 1]), valid[choice - 1]
            elif choice == len(valid) + 1:
                custom_path = input("Enter the custom remote path: ").strip()
                if fs.isdir(custom_path):
                    ds_name = posixpath.basename(custom_path.rstrip('/'))
                    return custom_path, ds_name
                else:
                    print("Invalid remote path. Please try again.")
            else:
                print("Invalid choice. Please try again.")
        except ValueError:
            print("Invalid input. Please enter a number.")

def analyze_complete_sampling_rate(accel_dfs, spi_gyro_df, i2c_gyro_df, save_dir="."):
    """
    Analyze and plot the sampling rate of all sensors.
    """
    import os as _os
    fig, axes = plt.subplots(1, 5, figsize=(25, 5))
    fig.suptitle('Sampling Rate Analysis - All Position Sensors', fontsize=16)
    
    sensor_names = ['Accelerometer 1', 'Accelerometer 2', 'Accelerometer 3', 'SPI Gyroscope', 'I2C Gyroscope']
    all_sensors = list(accel_dfs) + [spi_gyro_df, i2c_gyro_df]
    
    for i, (df, sensor_name) in enumerate(zip(all_sensors, sensor_names)):
        if df is not None:
            time_diff = df.index.to_series().diff().dt.total_seconds()
            with np.errstate(divide='ignore', invalid='ignore', over='ignore'):
                sampling_rate = 1.0 / time_diff
            sampling_rate = sampling_rate[np.isfinite(sampling_rate)]
            sampling_rate = sampling_rate[(sampling_rate > 0) & (sampling_rate < 1e6)]
            
            if i < 3:
                color, alpha = ['red', 'green', 'blue'][i], 0.7
            elif i == 3:
                color, alpha = 'magenta', 0.7
            else:
                color, alpha = 'cyan', 0.7
            
            axes[i].hist(sampling_rate, bins=100, edgecolor='black', color=color, alpha=alpha)
            axes[i].set_title(f'{sensor_name}')
            axes[i].set_xlabel('Sampling Rate (Hz)')
            axes[i].set_ylabel('Frequency')
            axes[i].set_xlim(0, 2000)
            axes[i].grid(True, alpha=0.3)
            
            print(f"\nSampling Rate Statistics - {sensor_name}:")
            print(f"  Mean: {sampling_rate.mean():.2f} Hz")
            print(f"  Median: {sampling_rate.median():.2f} Hz")
            print(f"  Std Dev: {sampling_rate.std():.2f} Hz")
            print(f"  Min: {sampling_rate.min():.2f} Hz")
            print(f"  Max: {sampling_rate.max():.2f} Hz")
        else:
            axes[i].text(0.5, 0.5, 'No Data', horizontalalignment='center', 
                        verticalalignment='center', transform=axes[i].transAxes, fontsize=14)
            axes[i].set_title(f'{sensor_name}')
            axes[i].set_xlabel('Sampling Rate (Hz)')
            print(f"\n{sensor_name}: No data available")
    
    plt.tight_layout()
    plt.savefig(_os.path.join(save_dir, "plot1.png"))

def _shade_dropouts(ax, df, dropouts, color='y', alpha=0.15):
    if df is None or not dropouts:
        return
    t0 = df.index[0]
    for d in dropouts:
        try:
            start_s = (d['start'] - t0).total_seconds()
            end_s = (d['end'] - t0).total_seconds()
        except Exception:
            continue
        if np.isfinite(start_s) and np.isfinite(end_s) and end_s > start_s:
            ax.axvspan(start_s, end_s, color=color, alpha=alpha, lw=0)

def plot_complete_sensor_data(accel_dfs, spi_gyro_df, i2c_gyro_df, dropout_results=None, save_dir="."):
    """
    Plot complete sensor data with optional dropout shading.
    """
    import os as _os
    fig, axes = plt.subplots(5, 3, figsize=(18, 20), sharex=True)
    fig.suptitle('Complete Position Sensor Data Analysis\n(3 Accelerometers + 1 SPI Gyroscope + 1 I2C Gyroscope)', fontsize=16)
    
    accel_names = ['Accelerometer 1', 'Accelerometer 2', 'Accelerometer 3']
    # Plot accelerometer data (first 3 rows)
    for i, (df, title) in enumerate(zip(accel_dfs, accel_names)):
        if df is not None:
            axes[i, 0].plot(df['Time_elapsed'].to_numpy(), df['X (m/s^2)'].to_numpy(), 'r-', linewidth=0.8)
            axes[i, 0].set_ylabel('Acceleration (m/s²)')
            axes[i, 0].set_title(f'{title} - X Axis')
            axes[i, 0].grid(True, alpha=0.3)
            axes[i, 1].plot(df['Time_elapsed'].to_numpy(), df['Y (m/s^2)'].to_numpy(), 'g-', linewidth=0.8)
            axes[i, 1].set_title(f'{title} - Y Axis')
            axes[i, 1].grid(True, alpha=0.3)
            axes[i, 2].plot(df['Time_elapsed'].to_numpy(), df['Z (m/s^2)'].to_numpy(), 'b-', linewidth=0.8)
            axes[i, 2].set_title(f'{title} - Z Axis')
            axes[i, 2].grid(True, alpha=0.3)
            if dropout_results and title in dropout_results:
                dlist = dropout_results[title].get('dropouts', [])
                _shade_dropouts(axes[i, 0], df, dlist)
                _shade_dropouts(axes[i, 1], df, dlist)
                _shade_dropouts(axes[i, 2], df, dlist)
        else:
            for j in range(3):
                axes[i, j].text(0.5, 0.5, 'No Data Available', 
                               horizontalalignment='center', verticalalignment='center',
                               transform=axes[i, j].transAxes, fontsize=12)
                axes[i, j].set_title(f'{title} - {"XYZ"[j]} Axis')
    
    # Plot SPI gyroscope data (4th row, only first subplot)
    if spi_gyro_df is not None:
        axes[3, 0].plot(spi_gyro_df['Time_elapsed'].to_numpy(), spi_gyro_df['Angular_Rate (deg/s)'].to_numpy(), 'm-', linewidth=0.8)
        axes[3, 0].set_ylabel('Angular Rate (deg/s)')
        axes[3, 0].set_title('SPI Gyroscope (ADXRS453) - Z Axis')
        axes[3, 0].grid(True, alpha=0.3)
        if dropout_results and 'SPI Gyroscope' in dropout_results:
            _shade_dropouts(axes[3, 0], spi_gyro_df, dropout_results['SPI Gyroscope'].get('dropouts', []))
    else:
        axes[3, 0].text(0.5, 0.5, 'No SPI Gyroscope Data', 
                       horizontalalignment='center', verticalalignment='center',
                       transform=axes[3, 0].transAxes, fontsize=12)
        axes[3, 0].set_title('SPI Gyroscope (ADXRS453) - Z Axis')
    
    axes[3, 1].set_visible(False)
    axes[3, 2].set_visible(False)
    
    # Plot I2C gyroscope data (5th row)
    if i2c_gyro_df is not None:
        axes[4, 0].plot(i2c_gyro_df['Time_elapsed'].to_numpy(), i2c_gyro_df['X (deg/s)'].to_numpy(), 'c-', linewidth=0.8)
        axes[4, 0].set_ylabel('Angular Rate (deg/s)')
        axes[4, 0].set_title('I2C Gyroscope (IAM20380HT) - X Axis')
        axes[4, 0].grid(True, alpha=0.3)
        axes[4, 1].plot(i2c_gyro_df['Time_elapsed'].to_numpy(), i2c_gyro_df['Y (deg/s)'].to_numpy(), 'orange', linewidth=0.8)
        axes[4, 1].set_title('I2C Gyroscope (IAM20380HT) - Y Axis')
        axes[4, 1].grid(True, alpha=0.3)
        axes[4, 2].plot(i2c_gyro_df['Time_elapsed'].to_numpy(), i2c_gyro_df['Z (deg/s)'].to_numpy(), 'purple', linewidth=0.8)
        axes[4, 2].set_title('I2C Gyroscope (IAM20380HT) - Z Axis')
        axes[4, 2].grid(True, alpha=0.3)
        if dropout_results and 'I2C Gyroscope' in dropout_results:
            dlist = dropout_results['I2C Gyroscope'].get('dropouts', [])
            _shade_dropouts(axes[4, 0], i2c_gyro_df, dlist)
            _shade_dropouts(axes[4, 1], i2c_gyro_df, dlist)
            _shade_dropouts(axes[4, 2], i2c_gyro_df, dlist)
    else:
        for j in range(3):
            axes[4, j].text(0.5, 0.5, 'No I2C Gyroscope Data', 
                           horizontalalignment='center', verticalalignment='center',
                           transform=axes[4, j].transAxes, fontsize=12)
            axes[4, j].set_title(f'I2C Gyroscope (IAM20380HT) - {"XYZ"[j]} Axis')
    
    for j in range(3):
        if axes[4, j].get_visible():
            axes[4, j].set_xlabel('Time (seconds)')
    
    plt.tight_layout()
    plt.savefig(_os.path.join(save_dir, "plot.png"))

def plot_temperature_data(i2c_gyro_df, save_dir="."):
    """
    Plot temperature data from I2C gyroscope separately.
    """
    import os as _os
    if i2c_gyro_df is not None and 'Temperature (°C)' in i2c_gyro_df.columns:
        fig, ax = plt.subplots(1, 1, figsize=(12, 4))
        ax.plot(i2c_gyro_df['Time_elapsed'].to_numpy(), i2c_gyro_df['Temperature (°C)'].to_numpy(), 'r-', linewidth=1)
        ax.set_xlabel('Time (seconds)')
        ax.set_ylabel('Temperature (°C)')
        ax.set_title('I2C Gyroscope Temperature Data')
        ax.grid(True, alpha=0.3)
        
        temp_mean = i2c_gyro_df['Temperature (°C)'].mean()
        temp_std = i2c_gyro_df['Temperature (°C)'].std()
        temp_range = i2c_gyro_df['Temperature (°C)'].max() - i2c_gyro_df['Temperature (°C)'].min()
        
        ax.text(0.02, 0.98, f'Mean: {temp_mean:.2f}°C\nStd: {temp_std:.3f}°C\nRange: {temp_range:.3f}°C', 
                transform=ax.transAxes, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        plt.tight_layout()
        plt.savefig(_os.path.join(save_dir, "plot2.png"))
        
        print(f"\nTemperature Statistics:")
        print(f"  Mean: {temp_mean:.3f}°C")
        print(f"  Std Dev: {temp_std:.3f}°C")
        print(f"  Min: {i2c_gyro_df['Temperature (°C)'].min():.3f}°C")
        print(f"  Max: {i2c_gyro_df['Temperature (°C)'].max():.3f}°C")
        print(f"  Range: {temp_range:.3f}°C")
    else:
        print("No temperature data available")

def _detect_dropouts_for_df(df, expected_rate_hz=None, threshold_factor=2.0):
    """
    Detect dropouts in a time-indexed DataFrame by inspecting gaps between consecutive timestamps.
    If expected_rate_hz is None, uses the median delta as the expected dt.
    Returns a dict with summary and a list of dropout records.
    """
    if df is None or len(df) < 2:
        return {
            'count': 0,
            'total_missing': 0,
            'worst_gap_s': 0.0,
            'expected_dt': None,
            'threshold_s': None,
            'dropouts': []
        }
    time_diff = df.index.to_series().diff().dt.total_seconds()
    diffs = time_diff.to_numpy()
    valid = diffs[np.isfinite(diffs)]
    if valid.size == 0:
        return {
            'count': 0,
            'total_missing': 0,
            'worst_gap_s': 0.0,
            'expected_dt': None,
            'threshold_s': None,
            'dropouts': []
        }
    median_dt = float(np.median(valid))
    expected_dt = (1.0 / float(expected_rate_hz)) if expected_rate_hz and expected_rate_hz > 0 else median_dt
    threshold_s = expected_dt * float(threshold_factor)

    dropout_idxs = np.where(diffs > threshold_s)[0]  # index i means gap between i-1 and i
    dropouts = []
    total_missing = 0
    worst_gap = 0.0

    for i in dropout_idxs:
        gap_s = float(diffs[i])
        start_ts = df.index[i - 1]
        end_ts = df.index[i]
        # estimate missing samples relative to expected dt
        est_missing = max(int(round(gap_s / expected_dt) - 1), 1)
        total_missing += est_missing
        worst_gap = max(worst_gap, gap_s)
        dropouts.append({
            'start': start_ts,
            'end': end_ts,
            'gap_seconds': gap_s,
            'estimated_missing_samples': est_missing
        })

    return {
        'count': int(len(dropout_idxs)),
        'total_missing': int(total_missing),
        'worst_gap_s': float(worst_gap),
        'expected_dt': float(expected_dt),
        'threshold_s': float(threshold_s),
        'dropouts': dropouts
    }

def _rolling_median_abs_deviation(s: pd.Series, window: int):
    med = s.rolling(window, center=True, min_periods=1).median()
    abs_dev = (s - med).abs()
    mad = abs_dev.rolling(window, center=True, min_periods=1).median()
    return med, mad

def despike_df(df: pd.DataFrame, columns, rate_hz: float, window_sec=0.05, n_sigmas=4.0, interpolate=True):
    """
    Hampel-type despiking per column:
    - window_sec: length of rolling window in seconds (short to catch spikes).
    - n_sigmas: threshold multiplier on MAD (scaled to ~sigma).
    - interpolate: if True, linearly interpolates removed spikes in time.
    Returns cleaned DataFrame and per-column replacement counts.
    """
    if df is None or rate_hz is None or rate_hz <= 0:
        return df, {}

    window = max(5, int(round(window_sec * rate_hz)) | 1)  # odd, >=5
    out = df.copy()
    report = {}

    for col in columns:
        if col not in out.columns:
            continue
        s = out[col].astype(float)
        med, mad = _rolling_median_abs_deviation(s, window)
        sigma_est = 1.4826 * mad
        thresh = n_sigmas * sigma_est + 1e-12
        mask = (s - med).abs() > thresh
        count = int(mask.sum())

        if count > 0:
            s_clean = s.mask(mask, np.nan)
            if interpolate and isinstance(out.index, pd.DatetimeIndex):
                s_clean = s_clean.interpolate(method='time', limit=window, limit_direction='both')
            else:
                s_clean = s_clean.where(~mask, med)
            out[col] = s_clean

        report[col] = {"replaced": count, "window": window}

    return out, report

def despike_all_sensors(accel_dfs, spi_gyro_df, i2c_gyro_df, expected_rates, window_sec=0.05, n_sigmas=4.0):
    """
    Apply despiking to all sensors. Returns cleaned (accel_dfs, spi_gyro_df, i2c_gyro_df)
    and writes a short report to outliers_report.txt.
    """
    lines = []
    cleaned_accels = []
    accel_names = ['Accelerometer 1', 'Accelerometer 2', 'Accelerometer 3']
    accel_cols = ['X (m/s^2)', 'Y (m/s^2)', 'Z (m/s^2)']

    for name, df in zip(accel_names, accel_dfs):
        rate = (expected_rates or {}).get(name)
        cleaned, rep = despike_df(df, accel_cols, rate, window_sec, n_sigmas, interpolate=True)
        cleaned_accels.append(cleaned)
        if rep:
            total = sum(v["replaced"] for v in rep.values())
            lines.append(f"{name}: replaced {total} spikes across X/Y/Z (window={next(iter(rep.values()))['window']})")
        else:
            lines.append(f"{name}: no data")

    # SPI gyro
    spi_name = 'SPI Gyroscope'
    if spi_gyro_df is not None:
        spi_rate = (expected_rates or {}).get(spi_name)
        spi_cleaned, rep = despike_df(spi_gyro_df, ['Angular_Rate (deg/s)'], spi_rate, window_sec, n_sigmas, interpolate=True)
        spi_gyro_df = spi_cleaned
        lines.append(f"{spi_name}: replaced {rep.get('Angular_Rate (deg/s)', {}).get('replaced', 0)} spikes")
    else:
        lines.append(f"{spi_name}: no data")

    # I2C gyro (do not touch Temperature)
    i2c_name = 'I2C Gyroscope'
    if i2c_gyro_df is not None:
        i2c_rate = (expected_rates or {}).get(i2c_name)
        i2c_cleaned, rep = despike_df(i2c_gyro_df, ['X (deg/s)', 'Y (deg/s)', 'Z (deg/s)'], i2c_rate, window_sec, n_sigmas, interpolate=True)
        i2c_gyro_df = i2c_cleaned
        total = sum(v["replaced"] for v in rep.values())
        lines.append(f"{i2c_name}: replaced {total} spikes across X/Y/Z")
    else:
        lines.append(f"{i2c_name}: no data")

    try:
        with open("outliers_report.txt", "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        print("\n".join(lines))
        print("Outlier report saved to: outliers_report.txt")
    except Exception as e:
        print(f"Could not save outlier report: {e}")

    return (tuple(cleaned_accels), spi_gyro_df, i2c_gyro_df)


def analyze_dropouts(accel_dfs, spi_gyro_df, i2c_gyro_df, threshold_factor=2.0, expected_rates=None, report_path="dropouts_report.txt"):
    """
    Analyze dropouts for all sensors and return per-sensor results.
    """
    sensors = [
        ('Accelerometer 1', accel_dfs[0]),
        ('Accelerometer 2', accel_dfs[1]),
        ('Accelerometer 3', accel_dfs[2]),
        ('SPI Gyroscope', spi_gyro_df),
        ('I2C Gyroscope', i2c_gyro_df),
    ]
    expected_rates = expected_rates or {}

    lines = []
    lines.append("=" * 80)
    lines.append(f"DROPOUT ANALYSIS (threshold_factor={threshold_factor})")
    lines.append("=" * 80)

    print("\n" + "=" * 80)
    print(f"DROPOUT ANALYSIS (threshold_factor={threshold_factor})")
    print("=" * 80)

    results = {}
    for name, df in sensors:
        rate = expected_rates.get(name)
        res = _detect_dropouts_for_df(df, expected_rate_hz=rate, threshold_factor=threshold_factor)
        results[name] = res

        if df is None or len(df) < 2:
            msg = f"{name}: No data"
            print(msg)
            lines.append(msg)
            continue

        msg = (f"{name}: dropouts={res['count']}, total_est_missing={res['total_missing']}, "
               f"worst_gap={res['worst_gap_s']:.6f}s, expected_dt={res['expected_dt']:.6f}s, "
               f"threshold={res['threshold_s']:.6f}s")
        print(msg)
        lines.append(msg)

        for d in res['dropouts'][:10]:
            detail = (f"  gap {d['gap_seconds']:.6f}s from {d['start']} -> {d['end']} "
                      f"(~{d['estimated_missing_samples']} missing)")
            lines.append(detail)
        if res['count'] > 10:
            lines.append(f"  ... {res['count'] - 10} more")

    try:
        with open(report_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        print(f"\nDropout report saved to: {report_path}")
    except Exception as e:
        print(f"Could not save dropout report: {e}")

    return results

def main():
    # Remote base directory with datasets
    remote_base = "/media/saggitarius/T7/position_tracking_data"

    # Connect SSH/SFTP  
    fs = RemoteFS(host=os.getenv("SAG_SSH_HOST", "localhost"),
                  username=os.getenv("SAG_SSH_USER", "saggitarius"),
                  password=os.getenv("SAG_SSH_PASS", None))
    try:
        fs.connect()
    except Exception as e:
        print(f"SSH connection failed: {e}")
        return
    try:
        # Select remote dataset folder
        folder_path, ds_name = select_remote_folder(fs, base_path=remote_base)
        print(f"Selected remote folder: {folder_path}")

        # Local figures/report dir
        figures_dir = os.path.join("figures", ds_name)
        os.makedirs(figures_dir, exist_ok=True)

        # Load all sensor data (remote)
        print("\nLoading sensor data...")
        print("Loading accelerometer data...")
        accel_df1, accel_df2, accel_df3 = load_and_process_accelerometer_files_remote(fs, folder_path)
        accel_dfs = (accel_df1, accel_df2, accel_df3)

        print("Loading SPI gyroscope data...")
        spi_gyro_df = load_and_process_spi_gyroscope_files_remote(fs, folder_path)

        print("Loading I2C gyroscope data...")
        i2c_gyro_df = load_and_process_i2c_gyroscope_files_remote(fs, folder_path)

        # Expected ODRs (firmware-configured) 
        # Note: These are approximate - actual rates may vary
        expected_rates = {
            'Accelerometer 1': 1000,
            'Accelerometer 2': 1000,
            'Accelerometer 3': 1000,
            'SPI Gyroscope': 500,  # Lower actual rate observed
            'I2C Gyroscope': 250,
        }

        # Despike
        print("\nDespiking short spikes...")
        accel_dfs, spi_gyro_df, i2c_gyro_df = despike_all_sensors(
            accel_dfs, spi_gyro_df, i2c_gyro_df, expected_rates, window_sec=0.05, n_sigmas=4.0
        )

        # Dropouts (use more generous threshold for real data)
        print("\nChecking for dropouts...")
        dropout_results = analyze_dropouts(
            accel_dfs, spi_gyro_df, i2c_gyro_df, threshold_factor=5.0, expected_rates=expected_rates,
            report_path=os.path.join(figures_dir, "dropouts_report.txt")
        )

        # Plots
        print("\nGenerating plots...")
        plot_complete_sensor_data(accel_dfs, spi_gyro_df, i2c_gyro_df, dropout_results=dropout_results, save_dir=figures_dir)
        analyze_complete_sampling_rate(accel_dfs, spi_gyro_df, i2c_gyro_df, save_dir=figures_dir)
        plot_temperature_data(i2c_gyro_df, save_dir=figures_dir)

        plt.show()

        # Summary
        print("\n" + "="*80)
        print("COMPLETE POSITION SENSOR DATASET SUMMARY")
        print("="*80)

        for i, df in enumerate(accel_dfs, 1):
            if df is not None:
                print(f"\nAccelerometer {i} (ADXL355):")
                print(f"  Total samples: {len(df):,}")
                print(f"  Time span: {df.index[-1] - df.index[0]}")
                print(f"  First timestamp: {df.index[0]}")
                print(f"  Last timestamp: {df.index[-1]}")
                print(f"  Average data rate: {len(df) / df['Time_elapsed'].iloc[-1]:.2f} Hz")
                print(f"  X-axis range: {df['X (m/s^2)'].min():.3f} to {df['X (m/s^2)'].max():.3f} m/s²")
                print(f"  Y-axis range: {df['Y (m/s^2)'].min():.3f} to {df['Y (m/s^2)'].max():.3f} m/s²")
                print(f"  Z-axis range: {df['Z (m/s^2)'].min():.3f} to {df['Z (m/s^2)'].max():.3f} m/s²")
            else:
                print(f"\nAccelerometer {i}: No data available")

        if spi_gyro_df is not None:
            print(f"\nSPI Gyroscope (ADXRS453):")
            print(f"  Total samples: {len(spi_gyro_df):,}")
            print(f"  Time span: {spi_gyro_df.index[-1] - spi_gyro_df.index[0]}")
            print(f"  First timestamp: {spi_gyro_df.index[0]}")
            print(f"  Last timestamp: {spi_gyro_df.index[-1]}")
            print(f"  Average data rate: {len(spi_gyro_df) / spi_gyro_df['Time_elapsed'].iloc[-1]:.2f} Hz")
            print(f"  Angular rate range: {spi_gyro_df['Angular_Rate (deg/s)'].min():.3f} to {spi_gyro_df['Angular_Rate (deg/s)'].max():.3f} deg/s")
        else:
            print(f"\nSPI Gyroscope: No data available")

        if i2c_gyro_df is not None:
            print(f"\nI2C Gyroscope (IAM20380HT):")
            print(f"  Total samples: {len(i2c_gyro_df):,}")
            print(f"  Time span: {i2c_gyro_df.index[-1] - i2c_gyro_df.index[0]}")
            print(f"  First timestamp: {i2c_gyro_df.index[0]}")
            print(f"  Last timestamp: {i2c_gyro_df.index[-1]}")
            print(f"  Average data rate: {len(i2c_gyro_df) / i2c_gyro_df['Time_elapsed'].iloc[-1]:.2f} Hz")
            print(f"  X-axis range: {i2c_gyro_df['X (deg/s)'].min():.3f} to {i2c_gyro_df['X (deg/s)'].max():.3f} deg/s")
            print(f"  Y-axis range: {i2c_gyro_df['Y (deg/s)'].min():.3f} to {i2c_gyro_df['Y (deg/s)'].max():.3f} deg/s")
            print(f"  Z-axis range: {i2c_gyro_df['Z (deg/s)'].min():.3f} to {i2c_gyro_df['Z (deg/s)'].max():.3f} deg/s")
            print(f"  Temperature range: {i2c_gyro_df['Temperature (°C)'].min():.2f} to {i2c_gyro_df['Temperature (°C)'].max():.2f} °C")
        else:
            print(f"\nI2C Gyroscope: No data available")
    finally:
        fs.close()

if __name__ == "__main__":
    main()
