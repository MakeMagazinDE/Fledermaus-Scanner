#!/usr/bin/python3

import serial
import paho.mqtt.client as mqtt #import the client1
broker_address="192.168.178.80"

print("creating new instance")
client = mqtt.Client("P1") #create new instance
print("connecting to broker")
client.connect(broker_address) #connect to broker


ser = serial.Serial('/dev/ttyACM0',115200)  # open serial port
print(ser.name)         # check which port was really used

out_array = {}
out_str = ""
send_array = {}
no_of_relevant_signals = 0;
idx = 0;

for i in range(0,32767):    # Prepare empty accumulation array of sufficient size
    send_array[i] = 0

while 1==1:

    value = str(ser.readline())              # read values from serial interface
    num_val = float(value[2:-5])
    if num_val<32000:                        # 32000 indicates that the whole FFT has been sent
        out_array[idx] = num_val
        idx = idx + 1

    if num_val == 32000:                     # Check if at least one of the FFT bins contains some signal
        signal_received = 0
        for i in range(0,len(out_array)-1):
            if out_array[i]>10:
                signal_received = 1;
                print(out_array[i]," at: ",i)

        if signal_received == 1:             # If one FFT bin contained a signal the whole dataset if printed and sent via MQTT
            no_of_relevant_signals = no_of_relevant_signals + 1         # Count those data setts which contain a signal of relevant strengths
            out_str = "[{\n \"series\": [\"C-Turm\"], \n\"data\": [ \n["
            for i in range(0,int(len(out_array)/2)):              # Only half of teh frequency range (up to 125kHz)
                if out_array[i] > send_array[i]:
                    send_array[i] = out_array[i]  # Accumulate OR Function
                out_str = out_str + "{ \"x\": "+ str(250000/1024*i) + ", \"y\": " + str(send_array[i]) + "},"   #  Divide by number of relevant signals captured

            out_str = out_str[0:-1] + "\n] \n], \n\"labels\": [\"\"] \n}]"    # Remove last Komma

            print(out_str);
            print("--------------------------------------------")

            client.publish("Fledermaus",out_str)                 # Here the whole FFT result is publish to MQTT

        idx=0;
        out_str = "";

        if no_of_relevant_signals>1000:           # Reset accumulated array after 1000 new relevant signals
            no_of_relevant_signals = 0;
            for i in range(0,len(send_array)):
                send_array[i]=0;

ser.close()             # close port - but this will never be reached because of the endless loop above
