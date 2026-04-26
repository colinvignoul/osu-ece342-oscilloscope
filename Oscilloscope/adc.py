from machine import ADC
import rp2
import array

# 1. Setup ADC
adc = ADC(26) # Initialize GPIO 26 as ADC
# Note: You may need to manually enable FIFO and DREQ via machine.mem32
# ADC_BASE = 0x4004c000
# machine.mem32[ADC_BASE + 0x08] = 0x1 | (1 << 3) # FCS: Enable FIFO and DREQ

# 2. Setup DMA
dest = array.array('H', [0] * 1000) # Destination buffer for 1000 samples (16-bit)
dma = rp2.DMA()
ctrl = dma.pack_ctrl(
    size=1,           # 16-bit transfers
    inc_read=False,   # Always read from same ADC FIFO address
    inc_write=True,   # Increment destination buffer address
    treq_sel=36       # DREQ_ADC constant for RP2040
)

# 3. Configure and Start
dma.config(
    read=0x4004c018,  # ADC FIFO address
    write=dest,
    count=len(dest),
    ctrl=ctrl,
    trigger=True
)