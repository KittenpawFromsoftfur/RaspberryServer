# Description
This program starts a server on your raspberry, that allows you to remote control your GPIO pins and your "Eight MOSFETS 8-Layer Stackable HAT for Raspberry Pi" from https://sequentmicrosystems.com/products/eight-mosfets-8-layer-stackable-card-for-raspberry-pi (not sponsored).

The program features a login system for security, and safety measures for individuals trying to circumvent it.
A secret phrase has to be entered for every registered account in order to unlock full functionality such as setting GPIOs. This avoids smart individuals from causing possible damage to your setup.

The following is the output of the 'help' function for a locked account.
```
> ****** Help ******
> All commands are case insensitive.
> Logged in as 'bob'.
>
> help... Lists this help. Usage: 'help'.
> register... Registers an account, 1-16 characters. Usage: 'register <username> <password>'. Example: 'register bob ross'
> logout... Logs an account out. Usage: 'logout'.
> echo... The echo which echoes. Usage: 'echo'.
> exit... Closes your connection. Usage: 'exit'.
> ******************
```

The following is the output of the 'help' function for an unlocked account.
```
> ****** Help ******
> All commands are case insensitive.
> Logged in as 'bob', your account is activated.
>
> help... Lists this help. Usage: 'help'.
> i... Activates your account and unlocks all commands. Usage: 'i give duck cookie'.
> register... Registers an account, 1-16 characters. Usage: 'register <username> <password>'. Example: 'register bob ross'
> logout... Logs an account out. Usage: 'logout'.
> define... Defines the name of an IO (GPIO range = 0-7, 10-16, 21-31; MOSFET range = 1-8). The name must have 1-16 characters and is case insensitive. Usage: 'define gpio/mosfet <IO-number> <name>'. Example: 'define gpio 0 fan'
> set... Sets the status of an IO (GPIO range = 0-7, 10-16, 21-31; MOSFET range = 1-8). All IOs are output. Multi setting IOs via name is not supported. Usage: 'set gpio/mosfet <IO-number/name> <1/0/on/off/high/low>'. Example: 'set gpio fan on'
> clear... Clears the status of all IOs. Usage: 'clear gpio/mosfet'. Example: 'clear mosfet'
> delete... Deletes the program log, your defines, your account or all files. Usage: 'delete log/defines/account/all'. Example: 'delete log'
> shutdown... Shuts down the server. Usage: 'shutdown'.
> run... Runs a console command on the machine. Usage: 'run <command>'. Example: 'run reboot'
> echo... The echo which echoes. Usage: 'echo'.
> exit... Closes your connection. Usage: 'exit'.
> ******************
```

# Raspberry setup
## Use "Raspberry Pi Imager" to Flash "PI OS 32 bit recommended" with the settings:
```
Enable SSH: Yes

The program has username-dependend filepaths. Either use the username "black" or modify the following files:
* main.h --> "FILEPATH_BASE ..."
* xserver.service --> "ExecStart=..."
* build --> "chmod ..."
```

## Install wiringPi
```
cd /tmp
wget https://project-downloads.drogon.net/wiringpi-latest.deb
sudo dpkg -i wiringpi-latest.deb
```

## Make server directory and grant permissions
```
mkdir /home/<username>/server
chmod +777 -R /home/<username>/server
```

## Set up a static IP address and Wifi
```
nano /etc/dhcpcd.conf
```

Add the following lines, replace the "static routers" IP with the default gateway of your router.
You can find out your router's default gateway via windows CMD "ipconfig /all" and look for the entry "Default Gateway" or via the linux command "ip route".
Keep in mind that with the subnet mask 24 (255.255.255.0), the IP of the raspberry has to match the default gateway of the router up to the mask. E.g. "192.168.178.x".
```
interface eth0
static ip_address=192.168.178.10/24
static routers=192.168.178.1
static domain_name_servers=192.168.178.1 8.8.8.8 fd51:42f8:caae:d92e::1

interface wlan0
static ip_address=192.168.178.20/24
static routers=192.168.178.1
static domain_name_servers=192.168.178.1 8.8.8.8 fd51:42f8:caae:d92e::1
```

## Install telnet
```
apt-get install telnet
```

## Add to autostart
Create link to xserver.service in autostart directory
```
ln -s /home/<username>/server/xserver.service /etc/systemd/system
```

Reload services daemon to make it aware of newly added service
```
systemctl daemon-reload
```

Enable the new service
```
systemctl enable xserver.service
```

Optionally you can start the service right now without having to restart the machine
```
systemctl start xserver.service
```

# Router setup
In your router configurations, forward the raspberry server port of the corresponding IP of eth0 or wlan0 IP.
Keep in mind, that having the same port forwarded by multiple IPs can lead to conflicts.

# Phone setup (Android)
Find out your router's public IP (Can change with time): https://www.whatismyip.com/.
Download app for raw TCP connection, e.g. "Termux" and type the following command:
```
telnet <public router ip> <raspberry server port>
```

The router looks what device has the port <raspberry server port> forwarded and routes the packets to it. Having multiple devices forward the same port can lead to conflicts.

# Other commands
## Remove from autostart
```
systemctl disable xserver.service
```
Keep in mind that disabling a service will remove the symlink from /etc/systemd/system/

## Stop service
```
systemctl stop xserver.service
```
This will only stop the service temporarily, rebooting the raspberry will start it again.

# Program call parameters
```
-p... Sets the server port (optional, default 8000)
-u... Enable uppercase response (optional, default 0)

Example 'server -p 8030 -u 1'
```