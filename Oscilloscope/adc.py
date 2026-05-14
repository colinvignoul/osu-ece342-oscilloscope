from machine import ADC, mem32, mem16
import uctypes 
import rp2
import array
import gc


# ADDRESSES -- from RP2040 datasheet and SDK

# ADC_BASE 0x4004c000
# DMA_BASE 0x50000000

# ADC CONFIG
# Offset   Name                Info
# 0x00     CS                  ADC Control and Status
# 0x04     RESULT              Result of most recent ADC conversion
# 0x08     FCS                 FIFO control and status
# 0x0c     FIFO                Conversion result FIFO
# 0x10     DIV                 Clock divider. If non-zero, CS_START_MANY will
# start conversions at regular intervals rather than back-to-back.
# The divider is reset when either of these fields are written.
# Total period is 1 + INT + FRAC / 256
# 0x14     INTR                Raw Interrupts
# 0x18     INTE                Interrupt Enable
# 0x1c     INTF                Interrupt Force
# 0x20     INTS                Interrupt status after masking & forcing

# DMA CONFIG
# Offset   Name                Info
# 0x000    CH0_READ_ADDR       DMA Channel 0 Read Address pointer
# 0x004    CH0_WRITE_ADDR      DMA Channel 0 Write Address pointer
# 0x008    CH0_TRANS_COUNT     DMA Channel 0 Transfer Count
# 0x00c    CH0_CTRL_TRIG       DMA Channel 0 Control and Status
# 0x010    CH0_AL1_CTRL        Alias for channel 0 CTRL register
# 0x014    CH0_AL1_READ_ADDR   Alias for channel 0 READ_ADDR register
# 0x018    CH0_AL1_WRITE_ADDR  Alias for channel 0 WRITE_ADDR register
# 0x01c    CH0_AL1_TRANS_COUNT_TRIG
# Alias for channel 0 TRANS_COUNT register
# This is a trigger register (0xc). Writing a nonzero value will
# reload the channel counter and start the channel.
# 0x020    CH0_AL2_CTRL        Alias for channel 0 CTRL register
# 0x024    CH0_AL2_TRANS_COUNT Alias for channel 0 TRANS_COUNT register
# 0x028    CH0_AL2_READ_ADDR   Alias for channel 0 READ_ADDR register
# 0x02c    CH0_AL2_WRITE_ADDR_TRIG
# Alias for channel 0 WRITE_ADDR register
# This is a trigger register (0xc). Writing a nonzero value will
# reload the channel counter and start the channel.
# 0x030    CH0_AL3_CTRL        Alias for channel 0 CTRL register
# 0x034    CH0_AL3_WRITE_ADDR  Alias for channel 0 WRITE_ADDR register
# 0x038    CH0_AL3_TRANS_COUNT Alias for channel 0 TRANS_COUNT register
# 0x03c    CH0_AL3_READ_ADDR_TRIG
# Alias for channel 0 READ_ADDR register
# This is a trigger register (0xc). Writing a nonzero value will
# reload the channel counter and start the channel.


# SOFTWARE SIDE
# set up memory
padded_allocation = bytearray(98304)

# move pointer up 64k, then round down to even 64k address
base_addr = uctypes.addressof(padded_allocation)
aligned_addr = (base_addr + 0xffff) & ~0xffff

# allocate storage space
destination = array.array('H', uctypes.bytes_at(aligned_addr, 65536))
gc.collect()


# HARDWARE SIDE
# configure DMA
DMA_BASE = 0x50000000  # addressmap
DMA_CHANNEL = DMA_BASE  # channel 0

# registers
DMA_READ = DMA_CHANNEL
DMA_WRITE = DMA_CHANNEL + 0x04
DMA_TRANSFER_COUNT = DMA_CHANNEL + 0x08
DMA_TRIG = DMA_CHANNEL + 0x0c

# set DMA to read from ADC output
mem32[DMA_READ] = ADC_BASE + 0x0c
mem32[DMA_WRITE] = aligned_addr
mem32[DMA_TRANSFER_COUNT] = 0xffffffff


# set up ADC
adc_0 = ADC(26)
adc_1 = ADC(27)

# configure ADC
ADC_BASE = 0x4004c000  # from addressmap.h
# ADC_FCS_EN and ADC_FCS_DREQ_EN
mem32[ADC_BASE + 0x08] = 0x1 | (1 << 3)  # fifo and dreq signal
# ADC_DIV_INT
mem32[ADC_BASE + 0x10] = 95 << 8  # sample rate set to 500ks/s


# for DMA: TREQ_SEL, RING_SEL, RING_SIZE, INCR_WRITE, DATA_SIZE, EN
mem32[DMA_TRIG] = (0x24 << 15) | (1 << 15) | (16 << 11) | (1 << 5) | (1<<2) | 1
# use ADC signal timing, ring buffer, incrementing, and turn on

# ADC_CS_RROBIN, ADC_CS_AINSEL, ADC_CS_EN
mem32[ADC_BASE] = (0x03 << 16) | (0 << 12) | (1 << 1)  # multiplexing/input/on
