import serial
import csv
import time

PORT = "COM3"
BAUD = 115200

ser = serial.Serial(
    PORT,
    BAUD
)

with open(
    "vibration_data.csv",
    "w",
    newline=""
) as file:

    writer = csv.writer(file)

    writer.writerow(
        [
            "timestamp",
            "x",
            "y",
            "z",
            "label"
        ]
    )

    while True:

        line = ser.readline()

        try:
            values = (
                line.decode()
                .strip()
                .split(",")
            )

            if len(values)==3:

                writer.writerow([
                    time.time(),
                    values[0],
                    values[1],
                    values[2],
                    "normal"
                ])

                print(values)

        except:
            pass