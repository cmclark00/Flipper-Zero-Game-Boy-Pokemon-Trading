#!/usr/bin/env python3
"""
Simple HTTP bridge for debugging
"""

import socket
import threading
import serial
import time

def handle_client(client_socket, ser):
    try:
        # Read HTTP request
        request = client_socket.recv(1024).decode('utf-8')
        print(f"Received request: {request.split()[0]} {request.split()[1]}")
        
        # Send to RP2040
        ser.write((request.split()[0] + " " + request.split()[1] + "\n").encode())
        ser.flush()
        
        # Wait for response
        response = ""
        start_time = time.time()
        while time.time() - start_time < 3:  # 3 second timeout
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if "HTTP/1.1" in line:
                    # Found start of HTTP response
                    response = line + "\r\n"
                    while True:
                        line = ser.readline().decode('utf-8', errors='ignore')
                        response += line
                        if len(line.strip()) == 0:  # End of headers
                            break
                    # Read content
                    while ser.in_waiting:
                        response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                        time.sleep(0.1)
                    break
            time.sleep(0.01)
        
        if response:
            client_socket.send(response.encode())
        else:
            client_socket.send(b"HTTP/1.1 504 Gateway Timeout\r\n\r\nRP2040 did not respond")
            
    except Exception as e:
        print(f"Error: {e}")
        client_socket.send(b"HTTP/1.1 500 Internal Server Error\r\n\r\nError occurred")
    finally:
        client_socket.close()

def main():
    # Connect to RP2040
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
    print("Connected to RP2040")
    
    # Create server socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('localhost', 5030))
    server.listen(5)
    
    print("Simple bridge listening on http://localhost:5030")
    
    while True:
        client, addr = server.accept()
        thread = threading.Thread(target=handle_client, args=(client, ser))
        thread.start()

if __name__ == "__main__":
    main()