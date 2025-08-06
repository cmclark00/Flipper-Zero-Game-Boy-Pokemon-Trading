#!/usr/bin/env python3
import serial
import http.server
import socketserver
import threading
import time

class RP2040Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            # Send request to RP2040
            ser = serial.Serial('/dev/ttyACM0', 115200, timeout=3)
            request_line = f"GET {self.path}"
            print(f"Sending to RP2040: {request_line}")
            
            ser.write((request_line + '\n').encode())
            ser.flush()
            
            # Read response
            response = ""
            start_time = time.time()
            in_response = False
            
            while time.time() - start_time < 5:  # 5 second timeout
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if "=== HTTP REQUEST" in line:
                        in_response = True
                        continue
                    elif line == "=== END HTTP RESPONSE ===":
                        break
                    elif in_response and line.startswith("HTTP/1.1"):
                        # Start collecting the HTTP response
                        response = line + "\r\n"
                        # Read rest of response
                        while ser.in_waiting or (time.time() - start_time < 5):
                            if ser.in_waiting:
                                chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                                response += chunk
                                if "=== END HTTP RESPONSE ===" in response:
                                    response = response.replace("=== END HTTP RESPONSE ===", "")
                                    break
                            time.sleep(0.01)
                        break
                time.sleep(0.01)
            
            ser.close()
            
            if response:
                # Send the response as-is
                self.wfile.write(response.encode())
            else:
                self.send_error(504, "RP2040 did not respond")
                
        except Exception as e:
            print(f"Error: {e}")
            self.send_error(500, str(e))

if __name__ == "__main__":
    with socketserver.TCPServer(("", 5030), RP2040Handler) as httpd:
        print("🌐 Serving on http://localhost:5030")
        print("📱 Open your browser and go to http://localhost:5030")
        httpd.serve_forever()