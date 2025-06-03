#!/bin/bash

#Configuration
LOCAL_FILE="/media/ophiuchus/T7/GPS_data.bin"
REMOTE_USER="saggitarius"
REMOTE_HOST="Saggitarius"
REMOTE_FILE="/media/saggitarius/T7/GPS_data/20240926_173005_GPS_data/gps_log_20240926_173005.bin"
SYNC_INTERVAL=1

sync_file() {
	rsync -az "$REMOTE_USER@$REMOTE_HOST:$REMOTE_FILE" "$LOCAL_FILE"
}

while true; do
	sync_file
	python3 /home/ophiuchus/get_GPS/get_GPS.py
	sleep "$SYNC_INTERVAL"
done
