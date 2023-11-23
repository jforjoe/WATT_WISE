import time
import socket 
import os
import struct
import xlsxwriter
from datetime import datetime

#=======================================

TCP_IP = '192.168.221.131'    
TCP_PORT = 80
BUFFER_SIZE = 1024

#=======================================

#IMPORTANT: All message should be terminated with "\r" character
MESSAGE_1 = b'1\r'  # power
#MESSAGE_1 = b"2\r"   # lighting

#=======================================

#IMPORTANT: All message should be terminated with "\r" character
#MESSAGE_2 = b"a\r"  # read total active energy
#MESSAGE_2 = b"b\r"  # read import active energy 
#MESSAGE_2 = b"c\r"  # read export active energy
#MESSAGE_2 = b"d\r"  # read total reactive energy
#MESSAGE_2 = b"e\r"  # read import reactive energy
#MESSAGE_2 = b"f\r"  # read export reactive energy
#MESSAGE_2 = b"g\r"  # read apparent energy
#MESSAGE_2 = b"h\r"  # read active power
#MESSAGE_2 = b"i\r"  # read reactive power
#MESSAGE_2 = b"j\r"  # read apparent power
#MESSAGE_2 = b"k\r"  # read voltage
#MESSAGE_2 = b"l\r"  # read current
#MESSAGE_2 = b"m\r"  # read power factor  
#MESSAGE_2 = b"n\r"  # read frequency
#MESSAGE_2 = b"o\r"  # read max demand active power
#MESSAGE_2 = b"p\r"  # read max demand reactive power
#MESSAGE_2 = b"q\r"  # read max demand apparent power
#MESSAGE_2 = b"r\r"  # read baud rate  
#MESSAGE_2 = b"s\r"  # read backlight
#MESSAGE_2 = b"t\r"  # read meterID
#MESSAGE_2 = b"u\r"  # backlight ON
#MESSAGE_2 = b"v\r"  # backlight OFF


#===============================================================================================================
current_datetime = datetime.now()

# Format the date in the desired way using strftime
formatted_date = current_datetime.strftime("%x")
#===============================================================================================================

# Following list contains commands to read voltage, current, frequency, power factor and power. This is for testing purpose
commandList = [
    [b"k\r", ' Volts\t'],
    [b"l\r", ' Amps\t'],
    [b"n\r", ' Hz\t'],
    [b"m\r", ' PF\t'],
    [b"h\r", ' Watts\t'],
    [b"a\r", ' kWh\t']
]

# =======================================
def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))

    # Log data into excel sheet
    workbook = xlsxwriter.Workbook("log_"+datetime.now().strftime("%d-%b-%Y_%H-%M-%S")+".xlsx")
    worksheet = workbook.add_worksheet()

    row = 4
    column = 2

    while True:
        try:
            #==============================================================================
            print(formatted_date)
            print(current_datetime.strftime("%X"))
            #==============================================================================
            
            for x in commandList:
                

                s.send(MESSAGE_1)
                s.send(x[0])  # Send MESSAGE_2 here. For repeat testing, I am sending MESSAGE_2 sets from the list

                # First receive status from server
                status = s.recv(BUFFER_SIZE).decode('utf-8')
                # Second, receive data
                dataString = s.recv(BUFFER_SIZE).decode('utf-8')


                if x[0] == b"h\r":  # if received data is power, convert Kilo-Watts into Watts
                    dataFloat = float(dataString)
                    dataFloat = dataFloat * 1000.0
                    dataString = str(dataFloat)

                if status == 'Success':
                    print(dataString + x[1], end='')
                    # Log data into excel sheet, only if the data received successfully
                    worksheet.write(row, column, float(dataString))
                    column = column + 1
                    print(status)
                else:
                    print(status)  # If data is not received successfully, print reason on screen for debug purpose
                    column = column + 1

                time.sleep(1.0)  # 1 second delay between each query
            print("\n")
            column = 2
            row = row + 1
            time.sleep(1.0)
        except KeyboardInterrupt:
            workbook.close()
            break
    s.close()

# =======================================
if __name__ == "__main__":
    os.system('cls' if os.name == 'nt' else 'clear')
    main()
# =======================================
