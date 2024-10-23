cmake_minimum_required(VERSION 3.29.0)

# Optional: print out extra messages to see what is going on. Comment it to have less verbose messages
# set(CMAKE_VERBOSE_MAKEFILE ON)

# Path to toolchain file. This one has to be before 'project()' below
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/arm-none-eabi-gcc.cmake)

# Setup project, output and linker file
project(STM32H5_HighCycleMem LANGUAGES C)

set(TARGET_H563ZI STM32H563ZI)

enable_language(C ASM)

# Build the executable based on the source files
add_executable(${TARGET_H563ZI} 
  src/stm32/startup_stm32h56x.S
  src/main.c
  src/flash.c)

set(CMAKE_EXECUTABLE_SUFFIX .elf)
set(EXECUTABLE_H563ZI ${TARGET_H563ZI}.elf)

# List of compiler defines, prefix with -D compiler option
target_compile_definitions(${TARGET_H563ZI} PRIVATE
  -D__HEAP_SIZE=0x0000
  -D__STACK_SIZE=0x0100
)

# List of include directories
target_include_directories(${TARGET_H563ZI} PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/stm32
  ${CMAKE_SOURCE_DIR}/cmsis
  ${ARM_TOOLCHAIN_DIR}/../arm-none-eabi/include
  ${ARM_TOOLCHAIN_DIR}/../arm-none-eabi/include/machine
  ${ARM_TOOLCHAIN_DIR}/../lib/cmsis/Include
  ${ARM_TOOLCHAIN_DIR}/../lib/gcc/arm-none-eabi/9.3.1/include
)

# Variant flags
set(CMAKE_C_FLAGS_DEBUG "-DDEBUG -O0")
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_ASM_FLAGS_DEBUG "-Wa,--gdwarf-2")
set(CMAKE_ASM_FLAGS_RELEASE "-Wa,--no-warn")

# Compiler options
target_compile_options(${TARGET_H563ZI} PRIVATE
  -mcpu=cortex-m33
  -mfloat-abi=hard
  -mtune=cortex-m33
  -mfpu=fpv5-sp-d16
  -mthumb
  -Wall
  -fdata-sections
  -ffunction-sections
  -g3
  -save-temps=obj
  -fno-strict-aliasing
)


# Linker options
target_link_options(${TARGET_H563ZI} PRIVATE
  -mcpu=cortex-m33
  -mfloat-abi=hard
  -mtune=cortex-m33
  -mfpu=fpv5-sp-d16
  -Wl,-script=${CMAKE_SOURCE_DIR}/src/stm32/stm32h56x_2M_boot.ld
  -Wl,-Map=${TARGET_H563ZI}.map
  -mthumb
  -Wl,--defsym=__HEAP_SIZE=0x0000
  -Wl,--defsym=__STACK_SIZE=0x0100
  -Wall
  -ffunction-sections
  -fdata-sections
  -O0
  -g3
  -save-temps=obj
  -fno-strict-aliasing
  -Wl,--print-memory-usage
  -Wl,--gc-sections
  -nostartfiles
  -specs=nosys.specs
)

# Optional: Print executable size as part of the post build process
add_custom_command(TARGET ${TARGET_H563ZI}
  POST_BUILD
  COMMAND ${CMAKE_SIZE_UTIL} ${EXECUTABLE_H563ZI})

# Optional: Create hex, bin and S-Record files after the build
add_custom_command(TARGET ${TARGET_H563ZI}
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O srec --srec-len=64 ${EXECUTABLE_H563ZI} ${TARGET_H563ZI}.s19
  COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXECUTABLE_H563ZI} ${TARGET_H563ZI}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE_H563ZI} ${TARGET_H563ZI}.bin)

