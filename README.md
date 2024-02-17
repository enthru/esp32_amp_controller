# esp32_amp_controller

This is started as simple power output monitoring device using tandem-match and ADS1115 with web interface.<br />
But then I also added some simple functions (and still adding) as measurement current (with ACS785), temerature (with Dallas sensor), efficiency and SWR.<br />
It also has a some protection functions (SWR, efficiency - to detect possible LPF problems - based on output power and current). Protection is disabling power using S50085A power switch.<br />
It can detect PTT via separate PTT pin, change color of web page header to red while transmitting and set PWM for fans at maximum.
<br /><br />


![alt text](https://enthru.net/wp-content/uploads/2024/02/image-6.png)
![alt text](https://enthru.net/wp-content/uploads/2024/02/image-7.png)
<br />
To connect to wi-fi it's using wifi-manager (https://github.com/tzapu/WiFiManager). So if esp32 is unable to connect to wi-fi it acts as AP and you can connect to it and setup wi-fi network configuration.
<br />
It has OTA firmware upgrade ability and can save setting to the EEPROM.
<br /><br />
Additional info and history: [My site](https://enthru.net/?tag=amp_controller)
<br /><br />
**Menu parameters description:**
<br />
<br />
**Max current** - maximum current before protection will disable power<br />
**Min coeff** - minimum efficency before protection will disable power<br />
**Max SWR** - maximum SWR before prottection triggered<br />
**Base PWM** - PWM value for non-ptt state (with PTT neabled it's maximum - 255)<br />
**Default enabled** - power state on boot<br />
**Voltage to calculate** coeff - actual input voltage<br />

**Alarm reset time** - to avoid EM interference and some possible issues with measuring (for me it happens pretty often, especially with high SWR) each time protection triggering it's not disabling power immediately it does measure 3 times and if all 3 times guard triggers - it switches to the alarm state and disables power. But if protection triggers once or two times but no more, when some time passed it resets counter. And that is the time between counter resets.<br />
**Web page** - interval between web page refreshes.<br />
**Protection check** - interval of protecition checks.<br />
**Temp check** - interval to check temp, dallas sensors doesn't like too frequent polling.<br />

 

