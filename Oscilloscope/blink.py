from machine import Pin, ADC
import utime 

shift_A = Pin(0, Pin.IN, Pin.PULL_DOWN)
shift_B = Pin(1, Pin.IN, Pin.PULL_DOWN)

scale_A = Pin(2, Pin.IN, Pin.PULL_DOWN)
scale_B = Pin(3, Pin.IN, Pin.PULL_DOWN)

trig_A = Pin(4, Pin.IN, Pin.PULL_DOWN)
scale_B = Pin(5, Pin.IN, Pin.PULL_DOWN)

def rotary_changed(sum):
    global prev_Value
    global value
    neg = False
    pos = False
    if sum == 0b1101 or sum == 0b0100 or sum == 0b0010 or sum == 0b1011: #Clockwise
        neg = True
    elif sum == 0b0001 or sum == 0b0111 or sum == 0b1110 or sum == 0b1000: #Counter-clockwise
        pos = True
    
    if pos:
        return '1'
    elif neg:
        return '0'
    else:
        return '2'

while True:
    prev_shift_A = shift_A
    prev_shift_B = shift_B
    prev_shift = (prev_shift_A.value() << 1) | prev_shift_B.value()
    shift_A = Pin(0, Pin.IN, Pin.PULL_DOWN)
    shift_B = Pin(1, Pin.IN, Pin.PULL_DOWN)
    cur_shift = (shift_A.value() << 1) | shift_B.value()
    if prev_shift != cur_shift:
        shift_sum = (prev_shift << 2) | cur_shift

