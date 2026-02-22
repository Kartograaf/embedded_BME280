The project was completed using a ESP32S3 microcontroller.
Download an unzip the Tasmota project files from https://github.com/arendst/Tasmota.
Ensure that the "xsns_120_mybme280.ino" is located in the /tasmota_xsns_sensor folder of the project
and that the "user_config_override" is pased into the /tasmota folder.

--This needs to be done on Python 3.11
Run the clean command:
pio run -e [environment] -t erase --upload-port [port number]
where [environment] is the model of the used microcontroller (esp32s3 for this project)
and the [port number] is the name of the port the device is connected to (eg. COM3 or /dev/ttyUSB1).

Run the build command:
pio run -e [environment]
to create the binaries.

Followed by the upload command:
pio run -e [environment] -t upload --upload-port [port number]
