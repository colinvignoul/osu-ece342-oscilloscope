from machine import Pin, ADC
import utime 

led = Pin("LED", Pin.OUT)

shift_A = Pin(0, Pin.IN, Pin.PULL_DOWN)
shift_B = Pin(1, Pin.IN, Pin.PULL_DOWN)

scale_A = Pin(2, Pin.IN, Pin.PULL_DOWN)
scale_B = Pin(3, Pin.IN, Pin.PULL_DOWN)

trig_A = Pin(4, Pin.IN, Pin.PULL_DOWN)
scale_B = Pin(5, Pin.IN, Pin.PULL_DOWN)

# This function is called if the rotary position changes. It takes 'sum' as an argument, which is a 4-bit integer.
#The first two bits are the previous value of the encoder; The last two bits are the current value
#It uses this integer to detect the direction of the rotary encoder's change
def rotary_changed(sum):
    # neg and pos keep track of direction. The function returns an integer based on direction
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
    led.toggle()
    utime.sleep(0.5)
    prev_shift_A = shift_A
    prev_shift_B = shift_B

    prev_shift = (prev_shift_A.value() << 1) | prev_shift_B.value()

    shift_A = Pin(0, Pin.IN, Pin.PULL_DOWN)
    shift_B = Pin(1, Pin.IN, Pin.PULL_DOWN)

    cur_shift = (shift_A.value() << 1) | shift_B.value()

    if prev_shift != cur_shift:
        shift_sum = (prev_shift << 2) | cur_shift
        direction_shift = rotary_changed(sum)
