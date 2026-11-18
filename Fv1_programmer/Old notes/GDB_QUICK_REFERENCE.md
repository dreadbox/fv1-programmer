# GDB Debugging Quick Reference

## Starting a Debug Session

### Terminal 1: Start OpenOCD Server
```bash
make debug
```
Keep this running - it connects to ST-Link and waits for GDB.

### Terminal 2: Connect GDB
```bash
make gdb
```

---

## Essential GDB Commands

### Program Control
```gdb
(gdb) monitor reset halt          # Reset MCU and halt
(gdb) load                         # Load program to flash
(gdb) continue                     # Resume execution (shortcut: c)
(gdb) step                         # Step into function (shortcut: s)
(gdb) next                         # Step over function (shortcut: n)
(gdb) finish                       # Run until current function returns
(gdb) until <line>                 # Run until line number
(gdb) quit                         # Exit GDB (shortcut: q)
```

### Breakpoints
```gdb
(gdb) break main                   # Break at main()
(gdb) break main.c:245            # Break at specific line
(gdb) break UpdateLED              # Break at function
(gdb) break main.c:245 if counter > 10  # Conditional breakpoint
(gdb) info breakpoints             # List all breakpoints (shortcut: i b)
(gdb) delete 1                     # Delete breakpoint #1
(gdb) delete                       # Delete all breakpoints
(gdb) disable 2                    # Temporarily disable breakpoint #2
(gdb) enable 2                     # Re-enable breakpoint #2
```

### Watchpoints (break when variable changes)
```gdb
(gdb) watch app_state              # Break when app_state changes
(gdb) watch *(uint32_t*)0x20000100 # Watch memory address
(gdb) info watchpoints             # List watchpoints
```

### Examining Variables
```gdb
(gdb) print variable_name          # Print variable (shortcut: p)
(gdb) print /x variable_name       # Print in hex
(gdb) print /t variable_name       # Print in binary
(gdb) print *pointer               # Dereference pointer
(gdb) print array[5]               # Array element
(gdb) print sizeof(struct)         # Size of type
(gdb) display counter              # Auto-display after each step
(gdb) undisplay 1                  # Stop auto-displaying
```

### Examining Memory
```gdb
(gdb) x/16xw 0x20000000           # Examine 16 words at address (hex)
(gdb) x/s string_ptr               # Examine as string
(gdb) x/4i main                    # Examine as 4 instructions
```

### Call Stack
```gdb
(gdb) backtrace                    # Show call stack (shortcut: bt)
(gdb) frame 2                      # Switch to frame #2
(gdb) info locals                  # Show local variables
(gdb) info args                    # Show function arguments
```

### Registers
```gdb
(gdb) info registers               # Show all registers
(gdb) print $pc                    # Program counter
(gdb) print $sp                    # Stack pointer
(gdb) set $r0 = 0x1234            # Modify register
```

### Source Code Navigation
```gdb
(gdb) list                         # Show source around current line (shortcut: l)
(gdb) list main                    # Show source of function
(gdb) list main.c:100              # Show specific line
(gdb) where                        # Show current location
```

---

## Typical Workflow Example

```gdb
# 1. Connect and load
(gdb) monitor reset halt
(gdb) load

# 2. Set breakpoints
(gdb) break main
(gdb) break HandleButtonPress

# 3. Run
(gdb) continue

# Program stops at main
(gdb) next                         # Step through initialization
(gdb) next
(gdb) next

# 4. Inspect variables
(gdb) print app_state
(gdb) watch app_state              # Break when it changes

# 5. Continue to next breakpoint
(gdb) continue

# Stopped at HandleButtonPress
(gdb) print status
(gdb) backtrace                    # See how we got here

# 6. Done debugging
(gdb) quit
```

---

## Useful Tips

### Reset and Restart
```gdb
(gdb) monitor reset halt
(gdb) continue
```

### View Disassembly
```gdb
(gdb) disassemble main
(gdb) set disassemble-next-line on
```

### Modify Variables During Debug
```gdb
(gdb) set var counter = 10
(gdb) set app_state = STATE_IDLE
```

### Save Breakpoints
```gdb
(gdb) save breakpoints breakpoints.gdb
(gdb) source breakpoints.gdb       # Restore later
```

### GDB Initialization File
Create `.gdbinit` in project root to auto-run commands:
```gdb
target extended-remote localhost:3333
monitor reset halt
load
break main
```

---

## Project-Specific Functions to Debug

- `main()` - Entry point
- `UpdateLED()` - LED state machine
- `CheckRamDiskStatus()` - RamDisk monitoring  
- `HandleButtonPress()` - Button handling
- `RamDisk_CommitToEEPROM()` - EEPROM write operation
- `EEPROM_WritePage()` - Low-level I2C write

---

## Common Issues

**"Cannot access memory at address"**
- MCU is running, halt it first: `monitor halt`

**Breakpoint not hit**
- Check optimization level (currently -Og is fine)
- Verify code was loaded: `monitor reset halt` then `load`

**Variables optimized out**
- Some variables may be in registers
- Try: `print $r0` (check registers)
- Reduce optimization in Makefile if needed
