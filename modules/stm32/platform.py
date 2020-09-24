import machine

# print bmp pinout
def pinout():
    for i in ['BMP_SWCLK', 'BMP_SWDIO', 'BMP_SWO', 'BMP_SRST', 'BMP_TDI', 'BMP_TRST', 'BMP_VTREF']:
        if hasattr(machine.Pin.board, i):
            p=getattr(machine.Pin.board, i, False)
            if p:
                 print(i+' '+p.name())
