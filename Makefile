BUILD_DIR := Build/Obj
HOST_DIR := Build
CC := gcc
LD := ld
NASM := nasm
HOST_SOURCES := Host/Treas.c Host/FileIo.c Host/FilePortal.c Host/GuestMemory.c \
	Host/LaunchImage.c Host/SharedChannel.c Host/VirtualMachine.c
HOST_HEADERS := Host/FileIo.h Host/FilePortal.h Host/GuestMemory.h Host/LaunchImage.h \
	Host/SharedChannel.h Host/VirtualMachine.h
USER_RUNTIME_OBJECTS := $(BUILD_DIR)/UserEntry.o $(BUILD_DIR)/NativeApi.o \
	$(BUILD_DIR)/Heap.o
HOST_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections \
	-fdata-sections -flto -Wl,--gc-sections -D_XOPEN_SOURCE=700 \
	-D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -IInclude

CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
	-fno-pic -fno-pie -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-ffunction-sections -fdata-sections -m64 -mno-red-zone -mcmodel=kernel \
	-mno-mmx -mno-sse -mno-sse2 -IInclude
USER_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding \
	-fno-stack-protector -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -ffunction-sections -fdata-sections -m64 -mno-red-zone \
	-mcmodel=large -mno-mmx -mno-sse -mno-sse2 -IInclude -IUser/Fetch
LDFLAGS := -nostdlib --gc-sections -z max-page-size=0x1000 -T Linker.ld
USER_LDFLAGS := -nostdlib --gc-sections -z max-page-size=0x1000 -T User/Linker.ld

.PHONY: all clean run test benchmark
all: $(BUILD_DIR)/Treas.elf $(HOST_DIR)/testapp.texb $(HOST_DIR)/fetch.texb $(HOST_DIR)/treas

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/BootEntry.o: Arch/X64/BootEntry.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/Interrupts.o: Arch/X64/Interrupts.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/UserMode.o: Arch/X64/UserMode.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/UserThreadThunk.o: Arch/X64/UserThreadThunk.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/Context.o: Arch/X64/Context.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/Init.o: User/Init.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/UserEntry.o: User/Runtime/Entry.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/NativeApi.o: User/Runtime/NativeApi.c Include/Treas/UserApi.h Include/Treas/Status.h Include/Treas/UserAbi.h Include/Treas/UserFile.h Include/Treas/UserObject.h Include/Treas/UserSystemInformation.h Include/Treas/UserVirtualMemory.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/Heap.o: User/Runtime/Heap.c Include/Treas/UserApi.h Include/Treas/UserVirtualMemory.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/Fetch.o: User/Fetch/Fetch.c User/Fetch/Fetch.h Include/Treas/UserApi.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/FetchConsole.o: User/Fetch/Console.c User/Fetch/Fetch.h Include/Treas/UserApi.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/FetchProcessor.o: User/Fetch/Processor.c User/Fetch/Fetch.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/UserThreadTest.o: Tests/UserThreadTest.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/UserThreadFaultTest.o: Tests/UserThreadFaultTest.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/UserThreadPreemptTest.o: Tests/UserThreadPreemptTest.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/SystemInformationProtectionTest.o: Tests/SystemInformationProtectionTest.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/SdkTest.o: Tests/SdkTest.c Include/Treas/UserApi.h Include/Treas/UserSystemInformation.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkStreamTest.o: Tests/SdkStreamTest.c Include/Treas/UserApi.h Include/Treas/UserSystemInformation.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkImageTest.o: Tests/SdkImageTest.c Include/Treas/UserApi.h Include/Treas/UserSystemInformation.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkReadOnlyTest.o: Tests/SdkReadOnlyTest.c Include/Treas/UserApi.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkNoExecuteTest.o: Tests/SdkNoExecuteTest.c Include/Treas/UserApi.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkMemoryTest.o: Tests/SdkMemoryTest.c Include/Treas/UserApi.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkFileTest.o: Tests/SdkFileTest.c Include/Treas/UserApi.h Include/Treas/UserFile.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/SdkObjectTest.o: Tests/SdkObjectTest.c Include/Treas/UserApi.h Include/Treas/UserObject.h | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(HOST_DIR)/testapp.texb: $(BUILD_DIR)/Init.o User/Linker.ld | $(BUILD_DIR)
	$(LD) $(USER_LDFLAGS) -o $@ $(BUILD_DIR)/Init.o

$(HOST_DIR)/fetch.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/Fetch.o $(BUILD_DIR)/FetchConsole.o $(BUILD_DIR)/FetchProcessor.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/Fetch.o $(BUILD_DIR)/FetchConsole.o $(BUILD_DIR)/FetchProcessor.o

$(HOST_DIR)/SdkTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkTest.o

$(HOST_DIR)/SdkStreamTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkStreamTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkStreamTest.o

$(HOST_DIR)/SdkImageTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkImageTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkImageTest.o

$(HOST_DIR)/SdkReadOnlyTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkReadOnlyTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkReadOnlyTest.o

$(HOST_DIR)/SdkNoExecuteTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkNoExecuteTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkNoExecuteTest.o

$(HOST_DIR)/SdkMemoryTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkMemoryTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkMemoryTest.o

$(HOST_DIR)/SdkFileTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkFileTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkFileTest.o

$(HOST_DIR)/SdkObjectTest.texb: $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkObjectTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_RUNTIME_OBJECTS) $(BUILD_DIR)/SdkObjectTest.o

$(HOST_DIR)/InvalidImage.texb: $(HOST_DIR)/SdkTest.texb Tests/CreateInvalidImage.py
	python3 Tests/CreateInvalidImage.py $< $@ program-header-bounds

$(HOST_DIR)/InvalidPermissions.texb: $(HOST_DIR)/SdkTest.texb Tests/CreateInvalidImage.py
	python3 Tests/CreateInvalidImage.py $< $@ writable-executable

$(HOST_DIR)/InvalidEntryPoint.texb: $(HOST_DIR)/SdkTest.texb Tests/CreateInvalidImage.py
	python3 Tests/CreateInvalidImage.py $< $@ entry-point

$(HOST_DIR)/UserThreadTest.texb: $(BUILD_DIR)/UserThreadTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(BUILD_DIR)/UserThreadTest.o

$(HOST_DIR)/UserThreadFaultTest.texb: $(BUILD_DIR)/UserThreadFaultTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(BUILD_DIR)/UserThreadFaultTest.o

$(HOST_DIR)/UserThreadPreemptTest.texb: $(BUILD_DIR)/UserThreadPreemptTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(BUILD_DIR)/UserThreadPreemptTest.o

$(HOST_DIR)/SystemInformationProtectionTest.texb: $(BUILD_DIR)/SystemInformationProtectionTest.o User/Linker.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(BUILD_DIR)/SystemInformationProtectionTest.o

$(HOST_DIR)/treas: $(HOST_SOURCES) $(HOST_HEADERS) Include/Treas/LaunchProtocol.h Include/Treas/SharedProtocol.h Include/Treas/Status.h Include/Treas/UserFile.h
	$(CC) $(HOST_CFLAGS) $(HOST_SOURCES) -o $@

$(BUILD_DIR)/KernelMain.o: Kernel/KernelMain.c Include/Treas/ImageLoader.h Include/Treas/Process.h Include/Treas/ProcessStartup.h Include/Treas/Pvh.h Include/Treas/Thread.h Include/Treas/Timer.h Include/Treas/UserThread.h Include/Treas/Types.h Include/Treas/Hal.h Include/Treas/Kernel.h Include/Treas/PhysicalMemory.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Thread.o: Kernel/Thread.c Include/Treas/Thread.h Include/Treas/UserThread.h Include/Treas/VirtualMemory.h Include/Treas/Types.h Include/Treas/Hal.h Include/Treas/Kernel.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/SpinLock.o: Kernel/SpinLock.c Include/Treas/SpinLock.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/UserThread.o: Kernel/UserThread.c Include/Treas/UserThread.h Include/Treas/Types.h Include/Treas/Kernel.h Include/Treas/SpinLock.h Include/Treas/PhysicalMemory.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ProcessStartup.o: Kernel/ProcessStartup.c Include/Treas/ProcessStartup.h Include/Treas/LaunchProtocol.h Include/Treas/Pvh.h Include/Treas/Types.h Include/Treas/PhysicalMemory.h Include/Treas/UserThread.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ImageLoader.o: Kernel/ImageLoader.c Include/Treas/ImageLoader.h Include/Treas/Pvh.h Include/Treas/Types.h Include/Treas/PhysicalMemory.h Include/Treas/UserThread.h Include/Treas/UserVirtualMemoryManager.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/BugCheck.o: Kernel/BugCheck.c Include/Treas/Types.h Include/Treas/Hal.h Include/Treas/Kernel.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/SystemCall.o: Kernel/SystemCall.c Include/Treas/Process.h Include/Treas/ProcessStartup.h Include/Treas/SystemCall.h Include/Treas/SystemService.h Include/Treas/Thread.h Include/Treas/Types.h Include/Treas/UserAbi.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/IoServices.o: Kernel/SystemServices/IoServices.c Include/Treas/Hal.h Include/Treas/SystemService.h Include/Treas/UserAbi.h Include/Treas/UserFile.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/MemoryServices.o: Kernel/SystemServices/MemoryServices.c Include/Treas/ProcessStartup.h Include/Treas/SystemService.h Include/Treas/UserVirtualMemoryManager.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ObjectServices.o: Kernel/SystemServices/ObjectServices.c Include/Treas/ObjectManager.h Include/Treas/SystemService.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ThreadServices.o: Kernel/SystemServices/ThreadServices.c Include/Treas/Process.h Include/Treas/ProcessStartup.h Include/Treas/SystemService.h Include/Treas/Thread.h Include/Treas/UserThread.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/SystemInformation.o: Kernel/SystemServices/SystemInformation.c Include/Treas/PhysicalMemory.h Include/Treas/SystemService.h Include/Treas/Timer.h Include/Treas/UserSystemInformation.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Timer.o: Kernel/Timer.c Include/Treas/Types.h Include/Treas/Hal.h Include/Treas/Timer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Trap.o: Kernel/Trap.c Include/Treas/Trap.h Include/Treas/SystemCall.h Include/Treas/Thread.h Include/Treas/VirtualMemory.h Include/Treas/Kernel.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/PhysicalMemory.o: Kernel/PhysicalMemory.c Include/Treas/Pvh.h Include/Treas/Types.h Include/Treas/SpinLock.h Include/Treas/PhysicalMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/VirtualMemory.o: Kernel/VirtualMemory.c Include/Treas/Types.h Include/Treas/SpinLock.h Include/Treas/PhysicalMemory.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/UserVirtualMemory.o: Kernel/UserVirtualMemory.c Include/Treas/PhysicalMemory.h Include/Treas/SpinLock.h Include/Treas/UserVirtualMemory.h Include/Treas/UserVirtualMemoryManager.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Process.o: Kernel/Process.c Include/Treas/Process.h Include/Treas/VirtualMemory.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ObjectManager.o: Kernel/ObjectManager.c Include/Treas/ObjectManager.h Include/Treas/SpinLock.h Include/Treas/Thread.h Include/Treas/UserObject.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/HalInterrupts.o: Hal/Interrupts.c Include/Treas/Hal.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Pic.o: Hal/Pic.c Hal/HalInternal.h Include/Treas/Hal.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Pit.o: Hal/Pit.c Hal/HalInternal.h Include/Treas/Hal.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Pci.o: Hal/Pci.c Hal/HalInternal.h Include/Treas/SharedProtocol.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/SharedChannel.o: Hal/SharedChannel.c Hal/HalInternal.h Include/Treas/Hal.h Include/Treas/SharedProtocol.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Platform.o: Hal/Platform.c Include/Treas/Hal.h Include/Treas/Types.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/Treas.elf: $(BUILD_DIR)/BootEntry.o $(BUILD_DIR)/Interrupts.o $(BUILD_DIR)/Context.o $(BUILD_DIR)/UserMode.o $(BUILD_DIR)/UserThreadThunk.o $(BUILD_DIR)/KernelMain.o $(BUILD_DIR)/Thread.o $(BUILD_DIR)/SpinLock.o $(BUILD_DIR)/UserThread.o $(BUILD_DIR)/Process.o $(BUILD_DIR)/ObjectManager.o $(BUILD_DIR)/ProcessStartup.o $(BUILD_DIR)/ImageLoader.o $(BUILD_DIR)/SystemCall.o $(BUILD_DIR)/IoServices.o $(BUILD_DIR)/MemoryServices.o $(BUILD_DIR)/ObjectServices.o $(BUILD_DIR)/ThreadServices.o $(BUILD_DIR)/SystemInformation.o $(BUILD_DIR)/Timer.o $(BUILD_DIR)/Trap.o $(BUILD_DIR)/BugCheck.o $(BUILD_DIR)/PhysicalMemory.o $(BUILD_DIR)/VirtualMemory.o $(BUILD_DIR)/UserVirtualMemory.o $(BUILD_DIR)/HalInterrupts.o $(BUILD_DIR)/Pic.o $(BUILD_DIR)/Pit.o $(BUILD_DIR)/Pci.o $(BUILD_DIR)/SharedChannel.o $(BUILD_DIR)/Platform.o Linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/BootEntry.o $(BUILD_DIR)/Interrupts.o $(BUILD_DIR)/Context.o $(BUILD_DIR)/UserMode.o $(BUILD_DIR)/UserThreadThunk.o $(BUILD_DIR)/KernelMain.o $(BUILD_DIR)/Thread.o $(BUILD_DIR)/SpinLock.o $(BUILD_DIR)/UserThread.o $(BUILD_DIR)/Process.o $(BUILD_DIR)/ObjectManager.o $(BUILD_DIR)/ProcessStartup.o $(BUILD_DIR)/ImageLoader.o $(BUILD_DIR)/SystemCall.o $(BUILD_DIR)/IoServices.o $(BUILD_DIR)/MemoryServices.o $(BUILD_DIR)/ObjectServices.o $(BUILD_DIR)/ThreadServices.o $(BUILD_DIR)/SystemInformation.o $(BUILD_DIR)/Timer.o $(BUILD_DIR)/Trap.o $(BUILD_DIR)/BugCheck.o $(BUILD_DIR)/PhysicalMemory.o $(BUILD_DIR)/VirtualMemory.o $(BUILD_DIR)/UserVirtualMemory.o $(BUILD_DIR)/HalInterrupts.o $(BUILD_DIR)/Pic.o $(BUILD_DIR)/Pit.o $(BUILD_DIR)/Pci.o $(BUILD_DIR)/SharedChannel.o $(BUILD_DIR)/Platform.o

run: all
	$(HOST_DIR)/treas -b $(HOST_DIR)/testapp.texb

benchmark: all
	python3 Tests/Benchmark.py

test: all $(HOST_DIR)/UserThreadTest.texb $(HOST_DIR)/UserThreadFaultTest.texb $(HOST_DIR)/UserThreadPreemptTest.texb $(HOST_DIR)/SystemInformationProtectionTest.texb $(HOST_DIR)/SdkTest.texb $(HOST_DIR)/SdkStreamTest.texb $(HOST_DIR)/SdkImageTest.texb $(HOST_DIR)/SdkReadOnlyTest.texb $(HOST_DIR)/SdkNoExecuteTest.texb $(HOST_DIR)/SdkMemoryTest.texb $(HOST_DIR)/SdkFileTest.texb $(HOST_DIR)/SdkObjectTest.texb $(HOST_DIR)/InvalidImage.texb $(HOST_DIR)/InvalidPermissions.texb $(HOST_DIR)/InvalidEntryPoint.texb
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/UserThreadTest.texb)" = "thread-ok"
	@$(HOST_DIR)/treas -b $(HOST_DIR)/UserThreadFaultTest.texb >/dev/null
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/UserThreadPreemptTest.texb)" = "preempt-ok"
	@$(HOST_DIR)/treas -b $(HOST_DIR)/SystemInformationProtectionTest.texb
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/SdkTest.texb)" = "sdk-ok"
	@printf 'hello\n' | $(HOST_DIR)/treas -b $(HOST_DIR)/SdkStreamTest.texb >$(BUILD_DIR)/SdkStreamTest.stdout 2>$(BUILD_DIR)/SdkStreamTest.stderr
	@test "$$(cat $(BUILD_DIR)/SdkStreamTest.stdout)" = "stdin:hello"
	@test "$$(cat $(BUILD_DIR)/SdkStreamTest.stderr)" = "stderr-ok"
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/SdkImageTest.texb)" = "image-ok"
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/SdkMemoryTest.texb)" = "memory-ok"
	@printf 'host-file-input\n' >$(BUILD_DIR)/HostFileInput
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/SdkFileTest.texb --read $(BUILD_DIR)/HostFileInput --write $(BUILD_DIR)/HostFileOutput)" = "file-ok"
	@cmp $(BUILD_DIR)/HostFileInput $(BUILD_DIR)/HostFileOutput
	@test "$$($(HOST_DIR)/treas -b $(HOST_DIR)/SdkObjectTest.texb)" = "object-ok"
	@Status=0; $(HOST_DIR)/treas -b $(HOST_DIR)/SdkReadOnlyTest.texb >/dev/null || Status=$$?; test $$Status -eq 127
	@Status=0; $(HOST_DIR)/treas -b $(HOST_DIR)/SdkNoExecuteTest.texb >/dev/null || Status=$$?; test $$Status -eq 127
	@Status=0; $(HOST_DIR)/treas -b $(HOST_DIR)/InvalidImage.texb >/dev/null || Status=$$?; test $$Status -eq 127
	@Status=0; $(HOST_DIR)/treas -b $(HOST_DIR)/InvalidPermissions.texb >/dev/null || Status=$$?; test $$Status -eq 127
	@Status=0; $(HOST_DIR)/treas -b $(HOST_DIR)/InvalidEntryPoint.texb >/dev/null || Status=$$?; test $$Status -eq 127
	@Output="$$($(HOST_DIR)/treas -b $(HOST_DIR)/fetch.texb)" && \
		printf '%s\n' "$$Output" | grep -q "Memory:"

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(HOST_DIR)/treas $(HOST_DIR)/testapp.texb $(HOST_DIR)/fetch.texb
	$(RM) $(HOST_DIR)/UserThreadTest.texb $(HOST_DIR)/UserThreadFaultTest.texb
	$(RM) $(HOST_DIR)/UserThreadPreemptTest.texb
	$(RM) $(HOST_DIR)/SystemInformationProtectionTest.texb
	$(RM) $(HOST_DIR)/SdkTest.texb
	$(RM) $(HOST_DIR)/SdkStreamTest.texb
	$(RM) $(HOST_DIR)/SdkImageTest.texb $(HOST_DIR)/InvalidImage.texb
	$(RM) $(HOST_DIR)/InvalidPermissions.texb $(HOST_DIR)/InvalidEntryPoint.texb
	$(RM) $(HOST_DIR)/SdkReadOnlyTest.texb $(HOST_DIR)/SdkNoExecuteTest.texb
	$(RM) $(HOST_DIR)/SdkMemoryTest.texb
	$(RM) $(HOST_DIR)/SdkFileTest.texb
	$(RM) $(HOST_DIR)/SdkObjectTest.texb
