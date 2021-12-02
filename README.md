# MKS-TinyBee
  - MKS TinyBee is a mainboard for 3d printing, based on ESP32 module. Support Marlin2.0 firmware, in addition to ordinary LCD2004, 12864 screens, it also supports MKS MINI12864 V3, and MKS TFT serial screen. The firmware can be updated via USB. Can be connected to WIFI, web page control printing. 
  - MKS TinyBee小蜜蜂主板，是一款集成ESP32 wifi模块的3D打印控制板。支持Marlin 2.0固件，除了支持普通的LCD2004,12864之外，还支持了MKS MINI12864 V3，并且，可以使用MKS TFT串口屏。固件更新可以使用USB直接下载，可以连接wifi进行网页控制打印。
  ![MKS TinyBee](https://github.com/makerbase-mks/MKS-TinyBee/blob/main/hardware/Image/MKS%20TinyBee%20V1.x.png)
  
# How to buy
MKS TinyBee: https://www.aliexpress.com/store/1047297?spm=a2g0o.detail.1000007.1.5d5d290apkP4kL

# Features
  - Support Marlin 2.0.x
  - Support LCD2004/12864, [MKS MINI12864 V3](https://www.aliexpress.com/store/group/LCD/1047297_516922172.html?spm=a2g0o.detail.0.0.497c6bdbl6nCQJ)
  - Support [MKS TFT](https://www.aliexpress.com/store/group/TFT/1047297_516922169.html?spm=a2g0o.store_pc_groupList.pcShopHead_8325768.1_1_0) series screen, MKS TFT24/28/32/35
  - Use USB upload firmware
  - Printing by connect wifi web control

# Board parameters
  - MCU is ESP32-WROOM-32U, Flash: 8192K, RAM: 520K, the frequency is: 240MHz 
  - Use power dc12~24V input
  - Support power reverse connection protection and power spike protection
  - Has 2 heater end+ 1 Heater BED, 3 NTC100K: TH1, TB, TH2(Need jumper selection)
  - 2 Channel and 2 power output xh2.54-2p interface
  - 5 axis 6 motor interface(Dual Z axis in parallel)
  - STEP/DIR moode and support external for high current drives
  - Microstep settings use DIP switch
  - Has X, Y, Z, MT_DET detection interface
  - Support [3D Touch](https://www.aliexpress.com/store/group/Auto-bed-leveling-senor/1047297_516621299.html?spm=a2g0o.store_pc_groupList.pcShopHead_8325768.1_2_3)
 
# PinMap
  | Items      |  Function  | Pin |
  |------------|------------|------------|
  | Heater |   H-BED | IO144 |
  | Heater | H-E0 | IO145 |
  | Heater | H-E1 | IO146 |
  | PWM FANs | FAN1 | IO147 |
  | PWM FANs | FAN2 | IO148 |
  | Steppers | X_ENABLE | IO128 |
  | Steppers | X_STEP | IO129 |
  | Steppers | X_DIR | IO130 |
  | Steppers | Y_ENABLE | IO131 |
  | Steppers | Y_STEP | IO132 |
  | Steppers | Y_DIR | IO133 |
  | Steppers | Z_ENABLE | IO134 |
  | Steppers | Z_STEP | IO135 |
  | Steppers | Z_DIR | IO136 |
  | Steppers | E0_ENABLE | IO137 |
  | Steppers | E0_STEP | IO138 |
  | Steppers | E0_DIR | IO139 |
  | Steppers | E1_ENABLE | IO140 |
  | Steppers | E1_STEP | IO141 |
  | Steppers | E1_DIR | IO142 |
  | Endstops | X_STOP | IO33 |
  | Endstops | Y_STOP | IO32 |
  | Endstops | Z_STOP | IO22 |
  | Detection  | MT_DET | IO35 |
  | Servo | 3D TOUCH | IO2 |
  | THM | TH1 | IO36 |
  | THM | TH2 | IO34(Need jumper selection) |
  | THM | TB | IO39 |
  | EXP1 | BEERPER | IO149 |
  | EXP1 | BTN_ENC | IO13 |
  | EXP1 | LCD_EN | IO21 |
  | EXP1 | LCD_RS | IO4 |
  | EXP1 | LCD_D4 | IO0 |
  | EXP1 | LCD_D5 | IO16 |
  | EXP1 | LCD_D6 | IO15 |
  | EXP1 | LCD_D7 | IO17 |
  | EXP2 | SD_MISO | IO19 |
  | EXP2 | SD_SCK | IO18 |
  | EXP2 | BTN_EN1 | IO14 |
  | EXP2 | SD_CS | IO5 |
  | EXP2 | BTN_EN2 | IO12 |
  | EXP2 | SD_MOSI | IO23 |
  | EXP2 | SD_DET | IO34(Default for SD_DET function) |
  | TF CARD | CS | IO5 |
  | TF CARD | SCK | IO18 |
  | TF CARD | MISO | IO19 |
  | TF CARD | MOSI | IO23 |
  | TF CARD | DET | IO34(Default for SD_DET function) |
  | USART2 | TXD2 | IO17 |
  | USART2 | RXD2 | IO16 |

# Wiring


# Firmware Config
  
  
  
  
  
  
