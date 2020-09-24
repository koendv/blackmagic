import network
def wifi_on(essid, passwd):
    import network
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print('connecting...')
        wlan.connect(essid, passwd)
        while not wlan.isconnected():
            pass
    print(wlan.ifconfig())

import machine, uos
def sdcard_on():
    machine.Pin(2).init(-1, machine.Pin.PULL_UP)
    uos.mount(machine.SDCard(width=4), "/sd")

def vbat():
    adc=machine.ADC(machine.Pin(34))
    vbat=adc.read()
    vbat*=3.7*2
    vbat/=4095
    return vbat

