run mosquitto in local machine(PC/ Raspberry pi) by, in one powershell terminal after setting path to cd/desktop/mosquitto
```
.\mosquitto_pub.exe -h localhost -t "mine/test" -m "KAVACHAM TEST"
```
in 2nd terminal

```
.\mosquitto_sub.exe -h localhost -t "mine/test" -v
```
Now flash esp32 with mqtt_test.ino
make changes in it, add wifi ssid, password, PC's IPv4 address,Port 1883 is allowed through the PC firewall.
ESP32 and PC are on the same Wi-Fi/network.

In the seconf terminal, hello message will be displayed after showing connection established
serial monitor in Arduino IDE will show mqtt message published
