# ESP32-S3 Audio Processor DAC - Full Project Architecture Diagram

## System Architecture Overview

```mermaid
graph TB
    subgraph "Hardware Layer"
        ADC[External ADC<br/>I2S Slave<br/>GPIO 35 DATA]
        DAC[External DAC<br/>I2S Master<br/>GPIO 37/38]
        LCD[ST7567S LCD<br/>128x64 Display<br/>I2C 0x3F<br/>GPIO 6/7]
        LED1[Green LEDs<br/>GPIO 39/40]
        LED2[Yellow LEDs<br/>GPIO 41/42]
        LED3[Red LEDs<br/>GPIO 1/2]
    end

    subgraph "ESP32-S3 Core"
        subgraph "I2S Peripherals"
            I2S_ADC[I2S_NUM_1<br/>ADC Slave Mode<br/>192kHz, 24-bit]
            I2S_DAC[I2S_NUM_0<br/>DAC Master Mode<br/>192kHz, 24-bit]
        end
        
        subgraph "FreeRTOS Tasks"
            AUDIO_TASK[Audio Task<br/>Priority: 5<br/>Stack: 4KB]
            LCD_TASK[LCD Task<br/>Priority: 3<br/>Stack: 4KB<br/>15 FPS]
        end
        
        subgraph "Audio Processing Pipeline"
            BUFFER_MGR[Audio Buffer Manager<br/>256 samples/channel<br/>8 DMA buffers]
            AUDIO_EFFECTS[Audio Effects<br/>Volume, Delay,<br/>Channel Swap]
            LED_CTRL[LED Control<br/>Intensity Analysis<br/>Smoothing]
        end
        
        subgraph "Display System"
            LCD_DRIVER[LCD Driver<br/>ST7567S Low-Level]
            WAVEFORM[Waveform Renderer<br/>Oscilloscope Mode]
        end
        
        subgraph "I2C Bus"
            I2C_BUS[I2C Master<br/>GPIO 6/7<br/>400kHz]
        end
    end

    subgraph "Main Application"
        MAIN[app_main<br/>Initialization<br/>& Configuration]
    end

    %% Hardware Connections
    ADC -->|I2S Data| I2S_ADC
    I2S_DAC -->|I2S Data| DAC
    I2C_BUS -->|I2C| LCD
    LED_CTRL -->|GPIO| LED1
    LED_CTRL -->|GPIO| LED2
    LED_CTRL -->|GPIO| LED3

    %% Software Connections
    MAIN -->|Init| AUDIO_TASK
    MAIN -->|Init| LCD_TASK
    MAIN -->|Init| LED_CTRL
    MAIN -->|Init| BUFFER_MGR
    
    AUDIO_TASK -->|Read| I2S_ADC
    AUDIO_TASK -->|Write| I2S_DAC
    AUDIO_TASK -->|Process| BUFFER_MGR
    AUDIO_TASK -->|Apply| AUDIO_EFFECTS
    AUDIO_TASK -->|Update| LED_CTRL
    
    BUFFER_MGR -.->|Zero-Copy<br/>Direct Access| LCD_TASK
    LCD_TASK -->|Render| WAVEFORM
    WAVEFORM -->|Draw| LCD_DRIVER
    LCD_DRIVER -->|I2C| I2C_BUS

    style ADC fill:#e1f5ff
    style DAC fill:#e1f5ff
    style LCD fill:#e1f5ff
    style LED1 fill:#e1f5ff
    style LED2 fill:#e1f5ff
    style LED3 fill:#e1f5ff
    style AUDIO_TASK fill:#fff4e1
    style LCD_TASK fill:#fff4e1
    style BUFFER_MGR fill:#e8f5e9
    style AUDIO_EFFECTS fill:#e8f5e9
    style LED_CTRL fill:#e8f5e9
```

## Data Flow Diagram

```mermaid
sequenceDiagram
    participant ADC as External ADC
    participant I2S_ADC as I2S ADC<br/>(Slave)
    participant AudioTask as Audio Task
    participant Buffer as Audio Buffer<br/>(Shared)
    participant Effects as Audio Effects
    participant LEDCtrl as LED Control
    participant LCDTask as LCD Task
    participant LCD as LCD Display
    participant I2S_DAC as I2S DAC<br/>(Master)
    participant DAC as External DAC

    Note over ADC,DAC: Audio Processing Pipeline (192kHz, 24-bit, Stereo)

    loop Continuous Audio Processing
        ADC->>I2S_ADC: Audio Samples (I2S)
        I2S_ADC->>AudioTask: i2s_read()<br/>256 samples/channel
        AudioTask->>Buffer: Store samples<br/>(int32_t array)
        
        par Parallel Processing
            AudioTask->>Effects: Process samples<br/>(Volume, Delay, Swap)
            Effects-->>Buffer: Modified samples
            AudioTask->>LEDCtrl: Calculate intensity
            LEDCtrl->>LEDCtrl: Update LED states<br/>(Green/Yellow/Red)
        and LCD Display
            LCDTask->>Buffer: Read samples<br/>(Zero-copy access)
            LCDTask->>LCDTask: Render waveform<br/>(15 FPS)
            LCDTask->>LCD: Display via I2C
        end
        
        AudioTask->>I2S_DAC: i2s_write()<br/>Processed samples
        I2S_DAC->>DAC: Audio Output (I2S)
    end
```

## Component Interaction Diagram

```mermaid
graph LR
    subgraph "Initialization Sequence"
        A[app_main] -->|1. I2C Scan| B[I2C Scanner]
        A -->|2. LCD Init| C[LCD Display Init]
        A -->|3. Show 'S'| C
        A -->|4. Start LCD Task| D[LCD Task Start]
        A -->|5. Init LED Control| E[LED Control Init]
        A -->|6. Test LEDs| E
        A -->|7. Init Audio Processor| F[Audio Processor Init]
        F -->|7a. Init Buffer Manager| G[Buffer Manager]
        F -->|7b. Init Audio Effects| H[Audio Effects]
        A -->|8. Start Audio Processor| F
        F -->|8a. Start Audio Task| I[Audio Task]
        I -->|8b. Init I2S ADC| J[I2S ADC]
        I -->|8c. Init I2S DAC| K[I2S DAC]
        A -->|9. Connect LCD to Buffer| D
        D -.->|Direct Access| G
    end
```

## Task Structure and Priorities

```mermaid
graph TD
    subgraph "FreeRTOS Task Hierarchy"
        MAIN_TASK[Main Task<br/>app_main<br/>One-time init]
        
        AUDIO_TASK[Audio Task<br/>Priority: 5<br/>Stack: 4KB<br/>Real-time processing]
        
        LCD_TASK[LCD Task<br/>Priority: 3<br/>Stack: 4KB<br/>15 FPS refresh]
        
        IDLE[Idle Task<br/>Priority: 0]
    end
    
    MAIN_TASK -->|Creates| AUDIO_TASK
    MAIN_TASK -->|Creates| LCD_TASK
    
    AUDIO_TASK -->|Runs continuously| AUDIO_TASK
    LCD_TASK -->|Runs at 15Hz| LCD_TASK
    
    style AUDIO_TASK fill:#ffcccc
    style LCD_TASK fill:#ccffcc
    style MAIN_TASK fill:#ccccff
```

## Memory and Buffer Architecture

```mermaid
graph TB
    subgraph "Memory Layout"
        HEAP[FreeRTOS Heap]
        
        subgraph "Audio Buffer (Shared)"
            AUDIO_BUF[Audio Buffer<br/>256 samples × 2 channels<br/>× 4 bytes = 2048 bytes<br/>int32_t array]
        end
        
        subgraph "I2S DMA Buffers"
            DMA_BUF1[DMA Buffer 1<br/>256 samples]
            DMA_BUF2[DMA Buffer 2<br/>256 samples]
            DMA_BUF3[DMA Buffer 3<br/>...]
            DMA_BUF8[DMA Buffer 8<br/>256 samples]
        end
        
        subgraph "LCD Frame Buffer"
            LCD_FB[LCD Frame Buffer<br/>128 × 64 pixels<br/>1024 bytes]
        end
    end
    
    HEAP -->|Allocates| AUDIO_BUF
    HEAP -->|Allocates| DMA_BUF1
    HEAP -->|Allocates| DMA_BUF2
    HEAP -->|Allocates| LCD_FB
    
    AUDIO_BUF -.->|Zero-Copy<br/>Direct Access| LCD_TASK
    AUDIO_TASK -->|Reads/Writes| AUDIO_BUF
    I2S_DMA -->|Uses| DMA_BUF1
    I2S_DMA -->|Uses| DMA_BUF2
```

## Hardware Pin Mapping

```mermaid
graph LR
    subgraph "ESP32-S3 GPIO Configuration"
        subgraph "I2S DAC (Master Output)"
            DAC_BCK[GPIO 38<br/>BCK]
            DAC_LRCK[GPIO 36<br/>LRCK]
            DAC_DATA[GPIO 37<br/>DATA]
        end
        
        subgraph "I2S ADC (Slave Input)"
            ADC_BCK[GPIO 38<br/>BCK Shared]
            ADC_LRCK[GPIO 36<br/>LRCK Shared]
            ADC_DATA[GPIO 35<br/>DATA]
        end
        
        subgraph "I2C LCD Display"
            LCD_SDA[GPIO 6<br/>SDA]
            LCD_SCL[GPIO 7<br/>SCL]
        end
        
        subgraph "LED Outputs"
            LED_G1[GPIO 39<br/>Green 1]
            LED_G2[GPIO 40<br/>Green 2]
            LED_Y1[GPIO 41<br/>Yellow 1]
            LED_Y2[GPIO 42<br/>Yellow 2]
            LED_R1[GPIO 2<br/>Red 1]
            LED_R2[GPIO 1<br/>Red 2]
        end
    end
    
    style DAC_BCK fill:#ffcccc
    style DAC_LRCK fill:#ffcccc
    style DAC_DATA fill:#ffcccc
    style ADC_DATA fill:#ccffcc
    style LCD_SDA fill:#ccccff
    style LCD_SCL fill:#ccccff
    style LED_G1 fill:#ffffcc
    style LED_G2 fill:#ffffcc
    style LED_Y1 fill:#ffffcc
    style LED_Y2 fill:#ffffcc
    style LED_R1 fill:#ffffcc
    style LED_R2 fill:#ffffcc
```

## Audio Processing Pipeline Detail

```mermaid
graph LR
    subgraph "Audio Processing Steps"
        STEP1[1. I2S Read<br/>From ADC<br/>192kHz, 24-bit]
        STEP2[2. Buffer Store<br/>int32_t array<br/>256 samples/ch]
        STEP3[3. Volume Scale<br/>×1.5]
        STEP4[4. Channel Swap<br/>Optional]
        STEP5[5. Delay Effect<br/>50ms, 90% wet<br/>40% feedback]
        STEP6[6. LED Intensity<br/>Calculate RMS<br/>Update LEDs]
        STEP7[7. I2S Write<br/>To DAC<br/>192kHz, 24-bit]
    end
    
    STEP1 --> STEP2
    STEP2 --> STEP3
    STEP3 --> STEP4
    STEP4 --> STEP5
    STEP5 --> STEP6
    STEP6 --> STEP7
    
    STEP2 -.->|Zero-Copy| LCD_TASK
    
    style STEP1 fill:#e1f5ff
    style STEP7 fill:#e1f5ff
    style STEP3 fill:#fff4e1
    style STEP4 fill:#fff4e1
    style STEP5 fill:#fff4e1
    style STEP6 fill:#e8f5e9
```

## Module Dependencies

```mermaid
graph TD
    MAIN[main.c] --> AUDIO_PROC[audio_processor.c]
    MAIN --> LCD_TASK_MOD[lcd_task.c]
    MAIN --> LED_CTRL_MOD[led_control.c]
    MAIN --> I2C_SCAN[i2c_scanner.c]
    MAIN --> LCD_DISP[lcd_display.c]
    
    AUDIO_PROC --> AUDIO_TASK_MOD[audio_task.c]
    AUDIO_PROC --> AUDIO_EFFECTS_MOD[audio_effects.c]
    AUDIO_PROC --> BUFFER_MGR_MOD[audio_buffer_manager.c]
    
    AUDIO_TASK_MOD --> I2S_CONFIG_MOD[i2s_config.c]
    AUDIO_TASK_MOD --> AUDIO_EFFECTS_MOD
    AUDIO_TASK_MOD --> BUFFER_MGR_MOD
    AUDIO_TASK_MOD --> LED_CTRL_MOD
    
    LCD_TASK_MOD --> LCD_DISP
    LCD_TASK_MOD --> BUFFER_MGR_MOD
    
    AUDIO_EFFECTS_MOD --> BUFFER_MGR_MOD
    
    style MAIN fill:#ffcccc
    style AUDIO_PROC fill:#ccffcc
    style AUDIO_TASK_MOD fill:#ccccff
    style LCD_TASK_MOD fill:#ffffcc
```

## Configuration Parameters

```mermaid
graph TB
    subgraph "Audio Configuration"
        AUDIO_CFG[audio_config_t<br/>volume_scale: 1.5<br/>enable_channel_swap: true<br/>enable_delay: true<br/>delay_time_ms: 50<br/>delay_mix: 0.9<br/>delay_feedback: 0.4]
    end
    
    subgraph "LCD Configuration"
        LCD_CFG[lcd_task_config_t<br/>sda_pin: 6<br/>scl_pin: 7<br/>lcd_contrast: 20<br/>samples_per_screen: 10000<br/>amplitude_scale: 100<br/>show_grid: true<br/>show_center_line: true]
    end
    
    subgraph "LED Configuration"
        LED_CFG[led_control_config_t<br/>low_threshold: 0.15<br/>medium_threshold: 0.35<br/>high_threshold: 0.65<br/>smoothing_factor: 0.2<br/>enable_leds: true]
    end
    
    subgraph "I2S Configuration"
        I2S_CFG[Sample Rate: 192kHz<br/>Bits: 24-bit in 32-bit<br/>Channels: 2 Stereo<br/>Buffer Size: 256<br/>DMA Buffers: 8]
    end
    
    AUDIO_CFG --> AUDIO_PROC
    LCD_CFG --> LCD_TASK
    LED_CFG --> LED_CTRL
    I2S_CFG --> I2S_CONFIG
```

## Key Features Summary

### Audio Processing
- **Sample Rate**: 192 kHz
- **Bit Depth**: 24-bit samples in 32-bit containers
- **Channels**: Stereo (2 channels)
- **Buffer Size**: 256 samples per channel
- **DMA Buffers**: 8 buffers for low latency

### Effects
- Volume scaling (1.5x default)
- Channel swapping
- Delay effect (50ms, 90% wet, 40% feedback)

### Display
- **Type**: ST7567S 128x64 LCD
- **Interface**: I2C (400 kHz)
- **Refresh Rate**: 15 FPS
- **Mode**: Oscilloscope waveform
- **Zero-Copy**: Direct buffer access

### LED Control
- **Green LEDs**: GPIO 39, 40 (low threshold: 15%)
- **Yellow LEDs**: GPIO 41, 42 (medium threshold: 35%)
- **Red LEDs**: GPIO 1, 2 (high threshold: 65%)
- **Smoothing**: 20% factor

### Task Priorities
- **Audio Task**: Priority 5 (real-time)
- **LCD Task**: Priority 3 (display)
- **Idle Task**: Priority 0 (background)

