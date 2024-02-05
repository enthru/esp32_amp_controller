# esp32_amp_controller

This is started as simple power output monitoring device using tandem-match and ADS1115 with web interface.<br />
But then I also added some simple functions as measurement current (with ACS785), temerature (with Dallas sensor), efficiency and SWR.<br />
It also has some simple protection functions, but actually I think it should be reworked, as not so reliable, anyway for my setup it works as planned (RD100 100Watt amp.). Protection is disabling power using S50085A power switch<br />

![alt text](https://enthru.net/wp-content/uploads/2024/02/esp3.jpg)
![alt text](https://enthru.net/wp-content/uploads/2024/02/esp4.jpg)

Menu parameters description:

Coeff delay - delay in milliseconds between efficency protection check. It means that protection check for efficency will happen once in this intervall<br />
Max current - maximum current before protection will disable power<br />
Min coeff - minimum efficency before protection will disable power<br />
Max SWR - maximum SWR before prottection triggered<br />
Base PWM - PWM value for non-ptt state (with PTT neabled it's maximum - 255)<br />
Default enabled - power state on boot<br />
