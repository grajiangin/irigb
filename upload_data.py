#!/usr/bin/env python3
"""
Upload data directory to ESP32 SPIFFS/LittleFS filesystem
Usage: python upload_data.py
"""

import os
import sys
import subprocess

def upload_data():
    """Upload data directory to ESP32 filesystem using PlatformIO"""
    project_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(project_dir, 'data')

    # Check if data directory exists
    if not os.path.exists(data_dir):
        print(f"Error: Data directory '{data_dir}' not found!")
        return False

    # Check if data directory has files
    if not os.listdir(data_dir):
        print(f"Warning: Data directory '{data_dir}' is empty!")
        return False

    print("Uploading data directory to ESP32 filesystem...")
    print(f"Data directory: {data_dir}")

    try:
        # Run PlatformIO uploadfs command
        cmd = ['pio', 'run', '--target', 'uploadfs']
        result = subprocess.run(cmd, cwd=project_dir, capture_output=True, text=True)

        if result.returncode == 0:
            print("✓ Successfully uploaded data to ESP32 filesystem!")
            print("Files uploaded:")
            for root, dirs, files in os.walk(data_dir):
                for file in files:
                    rel_path = os.path.relpath(os.path.join(root, file), data_dir)
                    print(f"  - {rel_path}")
            return True
        else:
            print("✗ Failed to upload data to ESP32 filesystem!")
            print("Error output:")
            print(result.stderr)
            return False

    except FileNotFoundError:
        print("Error: PlatformIO not found. Please install PlatformIO or add it to PATH.")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    success = upload_data()
    sys.exit(0 if success else 1)
