/*
 *arch/mips/loongson/ls1x-board/gpio.h
 *LOONSON LS1B GPIO driver - header file
 *
 *
 */
#ifndef _LS1B_GPIO_H_
#define _LS1B_GPIO_H_


#define GPIO_EN   0xbfd010c0
#define GPIO_DIR    0xbfd010d0
#define GPIO_IN_VAL 0xbfd010e0
#define GPIO_OUT_VAL 0xbfd010f0
#define GPIO_IRQ_BASE 0xbfd01070

#define GPIO_00     0
#define GPIO_01     1
#define GPIO_02     2
#define GPIO_03     3
#define GPIO_04     4
#define GPIO_05     5
#define GPIO_06     6
#define GPIO_07     7
#define GPIO_08     8
#define GPIO_09     9
#define GPIO_10    10 
#define GPIO_11    11 
#define GPIO_12    12
#define GPIO_13    13
#define GPIO_14    14
#define GPIO_15    15
#define GPIO_16    16
#define GPIO_17    17
#define GPIO_18    18
#define GPIO_19    19
#define GPIO_20    20
#define GPIO_21    21
#define GPIO_22    22
#define GPIO_23    23
#define GPIO_24    24
#define GPIO_25    25
#define GPIO_26    26
#define GPIO_27    27
#define GPIO_28    28
#define GPIO_29    29
#define GPIO_30    30
#define GPIO_31    31
#define GPIO_32    32
#define GPIO_33    33
#define GPIO_34    34
#define GPIO_35    35
#define GPIO_36    36
#define GPIO_37    37
#define GPIO_38    38
#define GPIO_39    39
#define GPIO_40    40
#define GPIO_41    41
#define GPIO_42    42
#define GPIO_43    43
#define GPIO_44    44
#define GPIO_45    45
#define GPIO_46    46
#define GPIO_47    47
#define GPIO_48    48
#define GPIO_49    49
#define GPIO_50    50
#define GPIO_51    51
#define GPIO_52    52
#define GPIO_53    53
#define GPIO_54    54
#define GPIO_55    55
#define GPIO_56    56
#define GPIO_57    57
#define GPIO_58    58
#define GPIO_59    59
#define GPIO_60    60
#define GPIO_61    61
 
#define GPIO_MAX  GPIO_61
#define GPIO_MIN  GPIO_00
#define GPIO_OUTPUT     1
#define GPIO_INPUT      0
#define GPIO_HIGH       1
#define GPIO_LOW        0

#define _reg_readl(x)         (*(volatile unsigned long *)(x))
#define _reg_writel(x,y)      ((*(volatile unsigned long *)(x))=(y))

#define PIN_IDX(pin)        ((pin)/32)
#define PIN_OFFSET(pin)     (0x1<<((pin)%32))
//#define GPIO_BIT(x)    (1<<(x))

#define LS1B_EDGE_FALL      0
#define LS1B_EDGE_UP        1
#define LS1B_LEVEL_HIGE     2
#define LS1B_LEVEL_LOW      3

extern int ls1b_gpio_register_pin(unsigned short pin);
extern int ls1b_gpio_init_register_pin(unsigned short pin,int output,int value);
extern int ls1b_gpio_unregister_pin(unsigned short pin);
extern int ls1b_gpio_read_input_pin(unsigned short pin);
extern int ls1b_gpio_read_output_pin(unsigned short pin);
extern int ls1b_gpio_write_pin(unsigned short pin, int output);
extern int ls1b_gpio_set_pin_direction(unsigned short pin, int output);
extern int ls1b_gpio_read_pin_direction(unsigned short pin);
extern int ls1b_gpio_to_irq(unsigned short pin);  
extern int ls1b_gpio_set_irq_type(unsigned short pin,unsigned char type);
extern void ls1b_gpio_change_edge(unsigned short irq);
extern void ls1b_gpio_set_interupt(unsigned short pin);
   
   
#endif
