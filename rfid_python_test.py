import serial
import time

# Configure the COM port and baud rate
ser = serial.Serial('COM4', 115200)  # Replace 'COMx' with your actual COM port (e.g., 'COM3' on Windows or '/dev/ttyUSB0' on Linux)
ser.timeout = 1  # Timeout for reading data (1 second)

# Open the serial port
if ser.is_open:
    print("Port is open")

# Hexadecimal command to send (as byte values)
command = bytes([0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0xFF, 0xFF, 0x4A, 0x7E])

# Define the expected header to search for (BB 02 22 00 11)
header = bytes([0xBB, 0x02, 0x22, 0x00, 0x11])

# Set to keep track of unique EPC data
unique_epcs = set()

try:
    while True:
        # Send the command to read multiple tags
        ser.write(command)
        print("Command sent to read tags.")

        # Read response from the UART device
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)  # Read all available data
            # print(f"Received: {data.hex().upper()}")  # Print the received data in hex format

            # Start processing only after the header is detected
            if header in data:
                # Find all occurrences of the header and split the data accordingly
                start = 0
                while True:
                    header_pos = data.find(header, start)
                    if header_pos == -1:  # No more headers found
                        break

                    # Extract the line data starting from the header
                    line_data = data[header_pos + len(header):]  # Data after the header
                    # Find the next header or the end of the data
                    next_header_pos = line_data.find(header)
                    if next_header_pos != -1:  # If there's another header, trim the data
                        line_data = line_data[:next_header_pos]

                    # Convert line data to hex string for easier manipulation
                    line_hex = line_data.hex().upper()

                    # Remove the first 3 bytes (6 characters) and last 4 bytes (8 characters) to get the EPC data
                    epc_data = line_hex[6:-8]  # Skip first 3 bytes (6 hex characters) and last 4 bytes (8 hex characters)

                    # Check if EPC data has exactly 12 bytes (24 hex characters)
                    if len(epc_data) == 24:
                        # Add the EPC data to the set to avoid duplicates
                        unique_epcs.add(epc_data)

                    # Update the start position for the next search
                    start = header_pos + 1

        # Print all unique EPC data
        if unique_epcs:
            print("Unique EPC Data:")
            for epc in unique_epcs:
                print(epc)

            print()
            unique_epcs.clear()  # Clear the set after printing

        time.sleep(1)  # Delay between sending commands (optional)

except KeyboardInterrupt:
    print("Closing connection...")
    ser.close()  # Close the serial connection when done
