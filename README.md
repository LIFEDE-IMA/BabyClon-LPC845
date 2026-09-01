# BabyClon-LPC845
LPC845 code & PCB schematics

NOT FINISHED, missing:
- DAC
- COMPARATOR

## NOTES:
### ETHERNET (W5500):
- MAC is currently hardcoded, check other devices in network before assigning one to the LPC.
- DNS server IP provided by DHCP does not work with all domains.
8.8.8.8 Google's DNS is currently hardcoded to resolve IPs.