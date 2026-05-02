from machine import Pin
from rotary_irq_rp2 import RotaryIRQ
import utime 


horz1_pos = 0
vert1_pos = 0
horz2_pos = 0
vert2_pos = 0

horz1_stretch = 1
vert1_stretch = 1
horz2_stretch = 1
vert2_stretch = 1

trig1_pos = 0
trig2_pos = 0

trig_shift = 0.05
graph_shift = 1
graph_scale = 1.25

voltage1 = -1
voltage2 = -0.5

draw1 = False
draw2 = False

led = Pin("LED", Pin.OUT)

shift_pins = [3, 4]
scale_pins = [5, 6]
trig_pins = [0, 1]
horz_switch_pin = 7
signal2_switch_pin = 2

shift = RotaryIRQ (
    pin_num_clk=shift_pins[0],
    pin_num_dt=shift_pins[1],
    range_mode = RotaryIRQ.RANGE_UNBOUNDED
    )

scale = RotaryIRQ (
    pin_num_clk=scale_pins[0],
    pin_num_dt=scale_pins[1],
    range_mode = RotaryIRQ.RANGE_UNBOUNDED
    )

trig = RotaryIRQ (
    pin_num_clk=trig_pins[0],
    pin_num_dt=trig_pins[1],
    range_mode = RotaryIRQ.RANGE_UNBOUNDED
    )


shift_old = shift.value()
scale_old = scale.value()
trig_old = trig.value()
counter = 0
trig1_ready = False
trig2_ready = False

while True:
    if (counter > 500):
        counter = 0
    counter += 1

    horz = Pin(horz_switch_pin, Pin.IN, Pin.PULL_DOWN).value()
    sig2 = Pin(signal2_switch_pin, Pin.IN, Pin.PULL_DOWN).value()

    shift_new = shift.value()
    scale_new = scale.value()
    trig_new =  trig.value()


    if (shift_new > shift_old):
        if horz and sig2:
            horz2_pos += graph_shift
        elif horz and (not sig2):
            horz1_pos += graph_shift
        elif (not horz) and sig2:
            vert2_pos += graph_shift
        else:
            vert1_pos += graph_shift

    elif (shift_new < shift_old):
        if horz and sig2:
            horz2_pos -= graph_shift
        elif horz and (not sig2):
            horz1_pos -= graph_shift
        elif (not horz) and sig2:
            vert2_pos -= graph_shift
        else:
            vert1_pos -= graph_shift

    if (scale_new > scale_old):
        if horz and sig2:
            horz2_stretch *= graph_scale
        elif horz and (not sig2):
            horz1_stretch *= graph_scale
        elif (not horz) and sig2:
            vert2_stretch *= graph_scale
        else:
            vert1_stretch *= graph_scale

    elif (scale_new < scale_old):
        if horz and sig2:
            horz2_stretch /= graph_scale
        elif horz and (not sig2):
            horz1_stretch /= graph_scale
        elif (not horz) and sig2:
            vert2_stretch /= graph_scale
        else:
            vert1_stretch /= graph_scale

    if (trig_new > trig_old):
        if sig2:
            trig2_pos += trig_shift
        else:
            trig1_pos += trig_shift

    elif (trig_new < trig_old):
        if sig2:
            trig2_pos -= trig_shift
        else:
            trig1_pos -= trig_shift


    shift_old = shift_new
    scale_old = scale_new
    trig_old = trig_new

    if(voltage1 < trig1_pos and trig1_ready == False):
        trig1_ready = True
    elif (voltage1 >= trig1_pos and trig1_ready == True):
        trig1_ready = False
        print("Trigger 1 Activated")

    if(voltage2 < trig2_pos and trig2_ready == False):
        trig2_ready = True
    elif (voltage2 >= trig2_pos and trig2_ready == True):
        trig2_ready = False
        print("Trigger 2 Activated")

    if(counter == 500):
        led.toggle()
        print("Vertical 1 Position:" + str(vert1_pos))
        print("Horizontal 1 Position:" + str(horz1_pos))
        print("Vertical 2 Position:" + str(vert2_pos))
        print("Horizontal 2 Position:" + str(horz2_pos))
        print("Vertical 1 scale:" + str(vert1_stretch))
        print("Horizontal 1 scale:" + str(horz1_stretch))
        print("Vertical 2 scale:" + str(vert2_stretch))
        print("Horizontal 2 scale:" + str(horz2_stretch))
        print("Trigger 1 Level: " + str(trig1_pos) + "  Voltage 1: " + str(voltage1))
        print("Trigger 2 Level: " + str(trig2_pos) + "  Voltage 2: " + str(voltage2))
    utime.sleep(.01)
    

    
