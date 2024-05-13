################################################################################
# MRS Version: 1.9.1
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include User/USB_Host/subdir.mk
-include User/subdir.mk
-include Startup/subdir.mk
-include Peripheral/src/subdir.mk
-include Debug/subdir.mk
-include Core/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
TK80.hex \

SECONDARY_LIST += \
TK80.lst \

SECONDARY_SIZE += \
TK80.siz \


# All Target
all: TK80.elf secondary-outputs

# Tool invocations
TK80.elf: $(OBJS) $(USER_OBJS)
	@	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized  -g -T "C:\MRS_DATA\workspace\TK80\Ld\Link.ld" -nostartfiles -Xlinker --gc-sections -Wl,-Map,"TK80.map" --specs=nano.specs --specs=nosys.specs -o "TK80.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@	@
TK80.hex: TK80.elf
	@	riscv-none-embed-objcopy -O ihex "TK80.elf"  "TK80.hex"
	@	@
TK80.lst: TK80.elf
	@	riscv-none-embed-objdump --all-headers --demangle --disassemble -M xw "TK80.elf" > "TK80.lst"
	@	@
TK80.siz: TK80.elf
	@	riscv-none-embed-size --format=berkeley "TK80.elf"
	@	@
# Other Targets
clean:
	-$(RM) $(ASM_UPPER_DEPS)$(OBJS)$(SECONDARY_FLASH)$(SECONDARY_LIST)$(SECONDARY_SIZE)$(ASM_DEPS)$(S_DEPS)$(S_UPPER_DEPS)$(C_DEPS) TK80.elf
	-@
secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets
