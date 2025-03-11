#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "i2s_mic_emulator.pio.h"

#define I2S_SD_PIN 10            // I2S serial data (SD)
#define I2S_SCK_PIN 11           // I2S serial clock / bit clock, (SCK) / (BCLK)
#define I2S_WS_PIN 12            // I2S word select/ left rigth clock, (WS) / (LRCLK)
#define SAMPLES 50
#define AMPLITUDE 8388607   // Max positive value for 24-bit signed
#define OFFSET (AMPLITUDE / 2)     // Offset to make values non-negative
#define SHIFT 8             // Shift left to MSB-align in a 32-bit word


int dma_channel;
uint32_t wavetable[SAMPLES];

void dma_handler(){
    dma_hw->ints0 = 1u << dma_channel;
    dma_channel_set_read_addr(dma_channel, wavetable, true);  
} 

int main(){
    stdio_init_all();
    for (int i = 0; i < SAMPLES; i++) {
        int32_t sample = (int32_t)(OFFSET + (OFFSET * sin(2.0 * M_PI * i / SAMPLES)));
        wavetable[i] = (uint32_t)sample << SHIFT; // Shift left to MSB-align
    }
   
    PIO pio;
    uint state_machine;
    uint offset; 
    bool success = pio_claim_free_sm_and_add_program( &i2s_mic_emulator_program, &pio, &state_machine, &offset);
    hard_assert(success);
    
    i2s_mic_emulator_program_init(pio, state_machine, offset, I2S_SD_PIN, I2S_SCK_PIN, I2S_WS_PIN);

    dma_channel = dma_claim_unused_channel(true);
    dma_channel_config dma_channel_cfg = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_channel_cfg, DMA_SIZE_32);
    channel_config_set_write_increment(&dma_channel_cfg, false);
    channel_config_set_read_increment(&dma_channel_cfg, true);
    channel_config_set_dreq(&dma_channel_cfg, pio_get_dreq(pio, state_machine, true));
    
    dma_channel_configure(
        dma_channel,
        &dma_channel_cfg,
        &pio->txf[state_machine],
        wavetable,
        SAMPLES,
        true
    );

    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    while (1) 
    // for (int i = 0; i < SAMPLES; i++) 
    //         printf("%d - %d \n", i, wavetable[i] );
    //     sleep_ms(1000);
        tight_loop_contents();
}