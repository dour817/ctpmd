################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/ctpmd.cpp \
../src/mdhandler.cpp \
../src/tdhandler.cpp 

OBJS += \
./src/ctpmd.o \
./src/mdhandler.o \
./src/tdhandler.o 

CPP_DEPS += \
./src/ctpmd.d \
./src/mdhandler.d \
./src/tdhandler.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/home/tcz/ctpapi -I/usr/local/include/bsoncxx/v_noabi -I/usr/local/include/mongocxx/v_noabi -O2 -Wall -c -fmessage-length=0 -std=gnu++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


