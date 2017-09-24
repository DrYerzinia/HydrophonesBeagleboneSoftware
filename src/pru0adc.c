#define TWO_CHANNEL_MODE
#define PGA_GAIN_1
#define MOD_GAIN_1

//  pru0_spi_read_test.c

#include "resource_table_0.h"
#include <am335x/pru_cfg.h>
#include <am335x/pru_intc.h>
#include <pru_rpmsg.h>
#include <rsc_types.h>
#include <stdint.h>
#include <stdio.h>

// Define remoteproc related variables.
#define HOST_INT ((uint32_t)1 << 30)

//  The PRU-ICSS system events used for RPMsg are defined in the Linux device tree.
//  PRU0 uses system event 16 (to ARM) and 17 (from ARM)
//  PRU1 uses system event 18 (to ARM) and 19 (from ARM)
#define TO_ARM_HOST 16
#define FROM_ARM_HOST 17

//  Using the name 'rpmsg-pru' will probe the rpmsg_pru driver found
//  at linux-x.y.x/drivers/rpmsg_pru.c
#define CHAN_NAME "rpmsg-pru"
#define CHAN_DESC "Channel 30"
#define CHAN_PORT 30

//  Used to make sure the Linux drivers are ready for RPMsg communication
//  Found at linux-x.y.z/include/uapi/linux/virtio_config.h
#define VIRTIO_CONFIG_S_DRIVER_OK 4

// Defines for PRU output states when timing critical
#define CONVRUN_BIT 7
#define SCLK_BIT    2
#define DOUT_BIT    1
#define DIN_BIT     0
#define CS_BIT      5
#define EOC_BIT     3

#ifdef TWO_CHANNEL_MODE
  #define READS_PER_BURST 400000
#elif FOUR_CHANNEL_MODE
  #define READS_PER_BURST 200000
#else
  #define READS_PER_PURST 200000
#endif

#define CFG_EXT_CLK   0x8000
#define CFG_CLKDIV1_2 0x0000
#define CFG_CLKDIV1_3 0x2000
#define CFG_CLKDIV1_4 0x4000
#define CFG_CLKDIV1_6 0x6000
#define CFG_SCHANA    0x0010
#define CFG_SCHANB    0x0008
#define CFG_SCHANC    0x0004
#define CFG_SCHAND    0x0002

#ifdef TWO_CHANNEL_MODE
  // TODO fix clock divider when we setup proper external crystal
  #define ADC_CFG CFG_EXT_CLK | CFG_CLKDIV1_2 | CFG_SCHANA | CFG_SCHANB
#elif FOUR_CHANNEL_MODE
  #define ADC_CFG CFG_EXT_CLK | CFG_CLKDIV1_3 | CFG_SCHANA | CFG_SCHANB | CFG_SCHANC | CFG_SCHAND
#else
  #define ADC_CFG CFG_EXT_CLK | CFG_CLKDIV1_3 | CFG_SCHANA | CFG_SCHANB | CFG_SCHANC | CFG_SCHAND
#endif

#define ADC_CFG_REG_ADDR  0x08
#define CH_A_CFG_REG_ADDR 0x0C
#define CH_B_CFG_REG_ADDR 0x0D
#define CH_C_CFG_REG_ADDR 0x0E
#define CH_D_CFG_REG_ADDR 0x0F

#define CH_CFG_NORM_OP 0x1000
#define CH_MOD_GAIN_2  0x0200
#define CH_MOD_GAIN_4  0x0400
#define CH_CFG_PGA_OFF 0x0010
#define CH_CFG_PGA_16  0x0004
#define CH_CFG_LP_FLT  0x0008
#define CH_CFG_BIASP   0x0002
#define CH_CFG_BIASN   0x0001

#ifdef PGA_GAIN_16
  #define CH_PGA_SET CH_CFG_PGA_16
#elif defined  PGA_GAIN_8
  #define CH_PGA_SET 0
#elif defined  PGA_GAIN_1
  #define CH_PGA_SET CH_CFG_PGA_OFF
#else
  #define CH_PGA_SET CH_CFG_PGA_OFF
#endif

#ifdef MOD_GAIN_1
  #define CH_MOD_GAIN_SET 0
#elif  MOD_GAIN_2
  #define CH_MOD_GAIN_SET CH_MOD_GAIN_2
#elif  MOD_GAIN_4
  #define CH_MOD_GAIN_SET CH_MOD_GAIN_4
#else
  #define CH_MOD_GAIN_SET 0
#endif

#define CH_SETTINGS CH_CFG_NORM_OP | CH_CFG_LP_FLT | CH_PGA_SET | CH_CFG_BIASP | CH_CFG_BIASN

#define CH_A_SETTINGS CH_SETTINGS
#define CH_B_SETTINGS CH_SETTINGS
#define CH_C_SETTINGS CH_SETTINGS
#define CH_D_SETTINGS CH_SETTINGS

//  Buffer used for PRU to ARM communication.
int8_t payload_pos = 0;
int32_t payload[120];

#define PRU_SHAREDMEM 0x00010000
volatile register uint32_t __R30;
volatile register uint32_t __R31;
uint32_t spiCommand;

struct pru_rpmsg_transport transport;
uint16_t src, dst, len;
volatile uint8_t *status;

// We have potential to remove 2 clock cycles and go from 20MHz SPI clock to 25MHz SPI clock
#pragma FUNC_ALWAYS_INLINE(read_cycle)
static inline void read_cycle(uint32_t *data){

  // Clock High, 3 extra cycles during read
  __R30 = __R30 | (1 << SCLK_BIT); // TODO correctbit? probably fine SET CLOCK HIGH, should  compile to single instruction, also deal with other IO states (1 cycle)
  *data = *data << 1;   // Shift data in preperation for next bit (1 cycle)
  __delay_cycles(3);

  // Clock Low
  __R30 = __R30 & ~(1 << SCLK_BIT); // Clock low (1 cycle)
   *data += (__R31 & 1); // Use R31_0 as MISO to save bitshift (2 cycles)
  __delay_cycles(2);

}

static void read_channels(){

  // Should be GP registers
  uint32_t data_1 = 0x00000000;
  #ifndef TWO_CHANNEL_MODE
    uint32_t data_2 = 0x00000000;
  #endif

  // Sample something into data because compiler is a dbag who optimizes thinks it shouldn't
  data_1 += (__R31 & 1);
  #ifndef TWO_CHANNEL_MODE
    data_2 += (__R31 & 1);
  #endif

  // Select ADC chip
  __R30 = __R30 & ~(1 << CS_BIT);

  #pragma UNROLL(32)
  for(int i = 0; i < 32; i++){
    read_cycle(&data_1);
  }

  #ifndef TWO_CHANNEL_MODE
    #pragma UNROLL(32)
    for(int i = 0; i < 32; i++){
      read_cycle(&data_2);
    }
  #endif

  // Deselect
  __R30 = __R30 | (1 << CS_BIT);

  payload[payload_pos++] = data_1;
  #ifndef TWO_CHANNEL_MODE
    payload[payload_pos++] = data_2;
  #endif

  if(payload_pos == 120){
    pru_rpmsg_send(&transport, dst, src, payload, 480);
    payload_pos = 0;
  }

}

// These commands are not timing critical and can be run at less than 20MHz
static uint16_t transfer_16(uint32_t addr, uint32_t dat, uint32_t r){

  uint32_t dout = ((((addr & 0x1F) << 2) + (r << 1)) << 24) + ((dat & 0xFF00) << 8) + ((dat & 0x00FF) << 8);
  uint16_t din;

  __R30 = __R30 & ~(1 << CS_BIT);

  for(int i = 1; i <= 24; i++){
    // TODO tune clock edges to be same length

    // SCLK HIGH
    __R30 = __R30 | (1 << SCLK_BIT);
    __delay_cycles(5);
    din <<= 1;  // shift DIN for next bit to be recieved
    dout <<= 1;
    if(dout & 0x80000000){
      __R30 = __R30 | (1 << DOUT_BIT);
    } else {
      __R30 = __R30 & ~(1 << DOUT_BIT);
    }

    // SCLK LOW
    __R30 = __R30 & ~(1 << SCLK_BIT);
    din += (__R31 >> DIN_BIT) & 0x01; // Read next inputbit
    __delay_cycles(5);

  }

  __R30 = __R30 & ~(1 << DOUT_BIT);
  __R30 = __R30 | (1 << CS_BIT);

  __delay_cycles(30);

  return din;

}

int main(void) {

  // Enable OCP Master Port
  CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

  //  Clear the status of PRU-ICSS system event that the ARM will use to 'kick' us
  CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

  //  Make sure the drivers are ready for RPMsg communication:
  status = &resourceTable.rpmsg_vdev.status;
  while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

  //  Initialize pru_virtqueue corresponding to vring0 (PRU to ARM Host direction).
  pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0, &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

  // Create the RPMsg channel between the PRU and the ARM user space using the transport structure.
  while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, CHAN_NAME, CHAN_DESC, CHAN_PORT) != PRU_RPMSG_SUCCESS);
  //  The above code should cause an RPMsg character to device to appear in the directory /dev.

  //  This section of code blocks until a message is received from ARM.
  while (pru_rpmsg_receive(&transport, &src, &dst, payload, &len) != PRU_RPMSG_SUCCESS) {}

  __R30 = __R30 & ~(1 << SCLK_BIT);
  __R30 = __R30 | (1 << CS_BIT);
  __R30 = __R30 & ~(1 << CONVRUN_BIT);
  __delay_cycles(2000);

  // Configure ADC registers
  transfer_16(ADC_CFG_REG_ADDR, ADC_CFG, 0); // EXT_CLK SCHAN 0xA01E
  __delay_cycles(200);
  transfer_16(CH_A_CFG_REG_ADDR, CH_A_SETTINGS, 0); // PDPGA OFF ENBIAS_P 0x101B
  __delay_cycles(200);
  transfer_16(CH_B_CFG_REG_ADDR, CH_B_SETTINGS, 0);
  __delay_cycles(200);
  transfer_16(CH_C_CFG_REG_ADDR, CH_C_SETTINGS, 0);
  __delay_cycles(200);
  transfer_16(CH_D_CFG_REG_ADDR, CH_D_SETTINGS, 0);
  __delay_cycles(200);

  // Start ADCs
  __R30 = __R30 | (1 << CONVRUN_BIT);

  //while (1) {
  for(int i = 0; i < READS_PER_BURST; i++){

    while((__R31 & (1 << EOC_BIT))); // Wait for EOC pulse
    read_channels();           // Read ADC channels and send data to ARM

    // TODO ARM interrupt for configuration change requests potentialy

  }

  __R30 = __R30 & ~(1 << CONVRUN_BIT);

  __halt();

}

