#!/usr/bin/env python3
"""
USB-Serial Bridge for RP2040 Zero Pokemon Trade Tool
This script creates a local HTTP server that bridges web requests to the RP2040 Zero via USB serial.
"""

import serial
import threading
import time
import http.server
import urllib.parse
import json
import sys
import os

class RP2040Bridge:
    def __init__(self, serial_port='/dev/ttyACM0', baud_rate=115200):
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self.ser = None
        self.response_buffer = ""
        self.response_event = threading.Event()
        self.response_ready = False
        
    def connect(self):
        try:
            self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=1)
            print(f"Connected to RP2040 Zero on {self.serial_port}")
            
            # Start reading thread
            self.read_thread = threading.Thread(target=self._read_serial, daemon=True)
            self.read_thread.start()
            
            return True
        except Exception as e:
            print(f"Failed to connect to RP2040 Zero: {e}")
            return False
    
    def _read_serial(self):
        """Background thread to read serial data"""
        in_response = False
        
        while True:
            try:
                if self.ser and self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if "=== HTTP REQUEST" in line:
                        in_response = True
                        self.response_buffer = ""
                        self.response_ready = False
                        continue
                    elif line == "=== END HTTP RESPONSE ===":
                        in_response = False
                        self.response_ready = True
                        self.response_event.set()
                        continue
                    
                    if in_response:
                        self.response_buffer += line + "\n"
                    else:
                        # Regular debug output
                        print(f"RP2040: {line}")
                        
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(1)
            
            time.sleep(0.01)
    
    def send_http_request(self, request_line):
        """Send HTTP request to RP2040 and wait for response"""
        if not self.ser:
            return "HTTP/1.1 500 Internal Server Error\r\n\r\nNot connected to RP2040"
        
        try:
            # Reset response state
            self.response_event.clear()
            self.response_ready = False
            self.response_buffer = ""
            
            # Send request
            self.ser.write((request_line + "\n").encode())
            self.ser.flush()
            
            # Wait for response with timeout
            if self.response_event.wait(timeout=5.0):
                return self.response_buffer
            else:
                return "HTTP/1.1 504 Gateway Timeout\r\n\r\nRP2040 did not respond in time"
                
        except Exception as e:
            return f"HTTP/1.1 500 Internal Server Error\r\n\r\nError: {e}"

class HTTPRequestHandler(http.server.BaseHTTPRequestHandler):
    def __init__(self, *args, bridge=None, **kwargs):
        self.bridge = bridge
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        """Handle GET requests"""
        # Create the request line that matches what the RP2040 expects
        request_line = f"GET {self.path}"
        
        print(f"Web request: {request_line}")
        
        # Send to RP2040 and get response
        response = self.bridge.send_http_request(request_line)
        
        # Parse the HTTP response from RP2040
        if response.startswith("HTTP/1.1"):
            # Full HTTP response
            self.wfile.write(response.encode())
        else:
            # Just content, add HTTP headers
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(response.encode())
    
    def log_message(self, format, *args):
        """Override to reduce verbose logging"""
        pass

def create_server(bridge, port=5030):
    """Create HTTP server with bridge"""
    def handler(*args, **kwargs):
        HTTPRequestHandler(*args, bridge=bridge, **kwargs)
    
    return http.server.HTTPServer(('localhost', port), handler)

def main():
    print("RP2040 Zero Pokemon Trade Tool - USB Bridge")
    print("=" * 50)
    
    # Find RP2040 serial port
    serial_port = '/dev/ttyACM0'
    if len(sys.argv) > 1:
        serial_port = sys.argv[1]
    
    # Create bridge
    bridge = RP2040Bridge(serial_port)
    
    if not bridge.connect():
        print("Failed to connect to RP2040. Make sure it's connected via USB.")
        sys.exit(1)
    
    # Create HTTP server
    port = 5030
    server = create_server(bridge, port)
    
    print(f"\\n🌐 Web UI Server started!")
    print(f"📱 Open your browser and go to: http://localhost:{port}")
    print(f"🔌 Connected to RP2040 on: {serial_port}")
    print(f"\\n💡 The web interface will show all Pokemon stored on your RP2040 Zero")
    print(f"🎮 Trade Pokemon with your Game Boy to see them appear in the web UI!")
    print(f"\\n⏹️  Press Ctrl+C to stop the server")
    print("=" * 50)
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\\n\\nShutting down server...")
        server.shutdown()
        if bridge.ser:
            bridge.ser.close()

if __name__ == "__main__":
    main()
