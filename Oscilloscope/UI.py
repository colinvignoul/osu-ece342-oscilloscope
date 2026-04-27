from machine import Pin
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
graph_shift = 2
graph_scale = 1.25

voltage1 = -1
voltage2 = -0.5

trigger1 = False
trigger2 = False

led = Pin("LED", Pin.OUT)

shift_A = Pin(0, Pin.IN, Pin.PULL_DOWN)
shift_B = Pin(1, Pin.IN, Pin.PULL_DOWN)
shift_pins = [0, 1]

scale_A = Pin(2, Pin.IN, Pin.PULL_DOWN)
scale_B = Pin(3, Pin.IN, Pin.PULL_DOWN)
scale_pins = [2, 3]

trig_A = Pin(4, Pin.IN, Pin.PULL_DOWN)
trig_B = Pin(5, Pin.IN, Pin.PULL_DOWN)
trig_pins = [4, 5]


horz_switch_pin = 6

signal2_switch_pin = 7

#Got this from google gemini
def get_pin_id(pin_obj):
    # Converts 'Pin(GPIO13, mode=IN)' or 'Pin(13)' into '13'
    import re
    # Extract digits from the string representation
    match = re.search(r'\d+', str(pin_obj))
    return int(match.group()) if match else None

    
def encoder_handler(pin: Pin):
    pin_id = get_pin_id(pin)
    pin_id2 = -1
    if pin_id:
        if (pin_id % 2) == 0:
            pin_id2 = pin_id + 1
        else:
            pin_id2 = pin_id - 1

    horz = Pin(horz_switch_pin, Pin.IN, Pin.PULL_DOWN).value() #Gets orientation of the horizontal/vertical switch
    sig2 = Pin(signal2_switch_pin, Pin.IN, Pin.PULL_DOWN).value() #Gets orientation of the signal 1/signal 2 switch

    if Pin(pin_id).value() == Pin(pin_id2).value():
        if horz and sig2:
            if pin_id in shift_pins:
                global horz2_pos
                horz2_pos += graph_shift
            elif pin_id in scale_pins:
                global horz2_stretch
                horz2_stretch *= graph_scale
            elif pin_id in trig_pins:
                global trig2_pos
                trig2_pos += trig_shift

        if (not horz) and sig2:
            if pin_id in shift_pins:
                global vert2_pos
                vert2_pos += graph_shift
            elif pin_id in scale_pins:
                global vert2_stretch
                vert2_stretch *= graph_scale
            elif pin_id in trig_pins:
                global trig2_pos
                trig2_pos += trig_shift

        if horz and (not sig2):
            if pin_id in shift_pins:
                global horz1_pos
                horz1_pos += graph_shift
            elif pin_id in scale_pins:
                global horz1_stretch
                horz1_stretch *= graph_scale
            elif pin_id in trig_pins:
                global trig1_pos
                trig1_pos += trig_shift

        if not (horz or sig2):
            if pin_id in shift_pins:
                global vert1_pos
                vert1_pos += graph_shift
            elif pin_id in scale_pins:
                global vert1_stretch
                vert1_stretch *= graph_scale
            elif pin_id in trig_pins:
                global trig1_pos
                trig1_pos += trig_shift
    else:
        if horz and sig2:
            if pin_id in shift_pins:
                global horz2_pos
                horz2_pos -= graph_shift
            elif pin_id in scale_pins:
                global horz2_stretch
                horz2_stretch /= graph_scale
            elif pin_id in trig_pins:
                global trig2_pos
                trig2_pos -= trig_shift

        if (not horz) and sig2:
            if pin_id in shift_pins:
                global vert2_pos
                vert2_pos -= graph_shift
            elif pin_id in scale_pins:
                global vert2_stretch
                vert2_stretch /= graph_scale
            elif pin_id in trig_pins:
                global trig2_pos
                trig2_pos -= trig_shift

        if horz and (not sig2):
            if pin_id in shift_pins:
                global horz1_pos
                horz1_pos -= graph_shift
            elif pin_id in scale_pins:
                global horz1_stretch
                horz1_stretch /= graph_scale
            elif pin_id in trig_pins:
                global trig1_pos
                trig1_pos -= trig_shift

        if not (horz or sig2):
            if pin_id in shift_pins:
                global vert1_pos
                vert1_pos -= graph_shift
            elif pin_id in scale_pins:
                global vert1_stretch
                vert1_stretch /= graph_scale
            elif pin_id in trig_pins:
                global trig1_pos
                trig1_pos -= trig_shift
    return
            

shift_A.irq(handler = encoder_handler, trigger=Pin.IRQ_FALLING|Pin.IRQ_RISING)
shift_B.irq(handler = encoder_handler, trigger=Pin.IRQ_FALLING|Pin.IRQ_RISING)
scale_A.irq(handler = encoder_handler, trigger=Pin.IRQ_FALLING|Pin.IRQ_RISING)
scale_B.irq(handler = encoder_handler, trigger=Pin.IRQ_FALLING|Pin.IRQ_RISING)
trig_A.irq(handler = encoder_handler, trigger=Pin.IRQ_FALLING|Pin.IRQ_RISING)
trig_B.irq(handler = encoder_handler, trigger=Pin.IRQ_FALLING|Pin.IRQ_RISING)


while True:
    #Test if Pico is working
    led.toggle()
    utime.sleep(5)
    trig1_ready = False
    trig2_ready = False

    if(voltage1 < trig1_pos & trig1_ready == False):
        trig1_ready = True
    elif (voltage1 >= trig1_pos & trig1_ready == True):
        trig1_ready = False
        print("Trigger 1 Activated")

    if(voltage2 < trig2_pos & trig2_ready == False):
        trig2_ready = True
    elif (voltage2 >= trig2_pos & trig2_ready == True):
        trig2_ready = False
        print("Trigger 2 Activated")

    print("Horizontal Sig1 Shift: " + str(horz1_pos))
    print("Vertical Sig1 Shift: " + str(vert1_pos))
    print("Horizontal Sig2 Shift: " + str(horz2_pos))
    print("Vertical Sig2 Shift: " + str(vert2_pos))
    print("Horizontal Sig1 Scale Factor: " + str(horz1_stretch))
    print("Vertical Sig1 Scale Factor: " + str(vert1_stretch))
    print("Horizontal Sig2 Scale Factor: " + str(horz2_stretch))
    print("Vertical Sig2 Scale Factor: " + str(vert2_stretch))
    print("Trigger 1 Level: " + str(trig1_pos) + "  Voltage 1: " + str(voltage1))
    print("Trigger 2 Level: " + str(trig2_pos) + "  Voltage 2: " + str(voltage2))
    

    
