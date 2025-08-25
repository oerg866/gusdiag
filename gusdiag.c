#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <conio.h>

#include "types.h"
#include "sys.h"
#include "vgacon.h"
#include "args.h"
#include "util.h"

#define __LIB866D_TAG__ "GUSDIAG"
#include "debug.h"

#define GUS_PORT_REGSEL 0x103
#define GUS_PORT_DATALO 0x104
#define GUS_PORT_DATAHI 0x105
#define GUS_PORT_DRAM   0x107
// #define GUS_PORT(base,off)

#define GUS_REG_RESET           0x4C
#define GUS_REG_SET_DRAM_LOW    0x43
#define GUS_REG_SET_DRAM_HIGH   0x44

static struct {
    bool    verbose;            // enable debug printouts
    bool    ignoreResetFail;    // Ignore reset/probe failure and test mem anyway.
    u16     address;            // 0 = auto
} s_config;

static void gus_writeReg8(u16 base, u8 reg, u8 val, bool dbgPrint) {
    outp(base+GUS_PORT_REGSEL, reg);
    outp(base+GUS_PORT_DATAHI, val);
    if (dbgPrint) DBG("GUS OUT %x %x %02x\n", base, reg, val);
}

static void gus_writeReg16(u16 base, u8 reg, u16 val, bool dbgPrint) {
    outp(base + GUS_PORT_REGSEL, reg);
    outpw(base + GUS_PORT_DATALO, val);
    if (dbgPrint) DBG("GUS OUT %x %x %04x\n", base, reg, val);
}

static u8 gus_readReg(u16 base, u8 reg, bool dbgPrint) {
    u8 val;
    outp(base + GUS_PORT_REGSEL, reg);
    val = inp(base + GUS_PORT_DATAHI);
    if (dbgPrint) DBG("GUS IN  %x %x %02x\n", base, reg, val);
    return val;
}

static u8 gus_dramRead(u16 base, u32 addr) {
    u32 addrLo = (addr & 0xFFFFUL);
    u32 addrHi = (addr >> 16) & 0xFFUL;
    L866_ASSERTM(addr <= 0xFFFFFFUL, "dram address out of range");
    gus_writeReg16(base, GUS_REG_SET_DRAM_LOW, (u16) addrLo, false);
    gus_writeReg8(base, GUS_REG_SET_DRAM_HIGH, (u8) addrHi, false);
    return inp(base+GUS_PORT_DRAM);
}

static void gus_dramWrite(u16 base, u32 addr, u8 val) {
    u32 addrLo = (addr & 0xFFFFUL);
    u32 addrHi = (addr >> 16) & 0xFFUL;
    L866_ASSERTM(addr <= 0xFFFFFFUL, "dram address out of range");
    gus_writeReg16(base, GUS_REG_SET_DRAM_LOW, (u16) addrLo, false);
    gus_writeReg8(base, GUS_REG_SET_DRAM_HIGH, (u8) addrHi, false);
    outp(base+GUS_PORT_DRAM, val);
}

static u32 gus_findMemSize(u16 base) {
#define MAX_RAM_TEST_SIZE (4UL*1024UL*1024UL)
    u32 addr;

    /* Pre-fill RAM with bit pattern to distinguish previously written values*/

    printf("      \xB3GUS @ 0x%03x: Memory test (Preparing...)\r", base);
    fflush(stdout);

    for (addr = 0; addr < 4UL*1024UL*1024UL; addr++) {
        gus_dramWrite(base, addr, 0xAA);
    }
    
    for (addr = 0; addr < 4UL*1024UL*1024UL; addr++) {
        u32 tstVal = ((addr >> 16) + 1UL) & 0xFFUL;
        u8 readBack;

        readBack = gus_dramRead(base, addr);
        
        if (readBack != 0xAA) {
            DBG("GUS @ 0x%03x: 0x%lx read invalid value, stopping memory test.", base, addr);
            break;
        }

        gus_dramWrite(base, addr, (u8) tstVal);
        readBack = gus_dramRead(base, addr);

        if (readBack != tstVal) {
            DBG("GUS @ 0x%03x: 0x%lx: Read value %02x does not match write value %02x", base, addr, (u16) readBack, (u16) tstVal);
            break;
        }

        if (addr % 1024UL == 0UL) {
            printf("      \xB3GUS @ 0x%03x: Memory test: %lu Bytes OK\r", base, addr);
            fflush(stdout);
        }
    }

    printf("\n");

    return addr;
}

static bool gus_probe(u16 base) {
    u8 resReg;

    vgacon_print("GUS Probe on Port %x\n", base);
    gus_writeReg8(base, GUS_REG_RESET, 0, true);
    resReg = gus_readReg(base, GUS_REG_RESET, true);
    
    
    if (0 != (resReg & 0x07)) {
        DBG("Card failed to ENTER reset state\n");
        return false;
    }

    sys_ioDelay(16000);
    gus_writeReg8(base, GUS_REG_RESET, 1, true);
    sys_ioDelay(16000);

    resReg = gus_readReg(base, GUS_REG_RESET, true);
    
    if (1 != (resReg & 0x07)) {
        DBG("Card failed to EXIT reset state\n");
        return false;
    }

    vgacon_printOK("GUS @ 0x%03x: Reset OK!\n", base);

    return true;    
}

static bool gus_testOne(u16 base, bool ignoreResetFail) {
    bool probeStatus = gus_probe(base);
    u32 memSize = 0UL;

    if (!probeStatus && !ignoreResetFail) {
        return false;
    }

    if (!probeStatus && ignoreResetFail) {
        vgacon_printWarning("GUS @ 0x%03x: Probe/Reset failed - OVERRIDE, testing anyway\n", base);
    }

    memSize = gus_findMemSize(base);

    if (memSize == 0UL) {
        vgacon_printWarning("GUS @ 0x%03x: No/bad memory or defective DRAM interface!\n", base);
    }
        
    vgacon_printOK("GUS @ 0x%03x: Card test successful with %lu (0x%lx) bytes of RAM.\n", base, memSize);

    return true;
}

static int gus_testAllCards(bool ignoreResetFail) {
    u16 base;
    int cardsFound = 0;

    for (base = 0x220; base <= 0x280; base += 0x20) {
        if (gus_testOne(base, ignoreResetFail)) {
            cardsFound++;
        }
    }
    return cardsFound;
}

static const char c_programName[] = "GUSDIAG Version 0.1 (C) 2025 E. Voirin (oerg866@googlemail.com)";

static const args_arg s_argList[] = {
    ARGS_HEADER(c_programName, "A Diagnostic tool for Gravis Ultrasound Classic cards."),
    ARGS_USAGE("?", "Prints parameter list"),

    { "v",          NULL,               "Extra verbose program output",                         ARG_FLAG,               NULL,                       &s_config.verbose,          NULL },
    { "a",          "address",          "Probe card specifically at this address",              ARG_U16,                NULL,                       &s_config.address,          NULL },
                            ARGS_EXPLAIN("Example: -a:0x240"),
    { "i",          NULL,               "Ignore Reset/Probe fail and test anyway",              ARG_FLAG,               NULL,                       &s_config.ignoreResetFail,  NULL },
                            ARGS_EXPLAIN("It's recommended to only use this with -a."),
};

int main(int argc, const char *argv[]) {
    bool ok = true;
    u32 memSize;
    args_ParseError argStatus;

    vgacon_setLogLevel(VGACON_LOG_LEVEL_INFO);
    
    memset(&s_config, 0, sizeof(s_config));

    argStatus = args_parseAllArgs(argc, argv, s_argList, ARRAY_SIZE(s_argList));

    if (argStatus == ARGS_USAGE_PRINTED) {
        return 0;
    }

    if (argStatus != ARGS_NO_ARGUMENTS && argStatus != ARGS_SUCCESS) {
        vgacon_printError("Command line error, aborting.\n");
        return -1;
    }

    vgacon_print("%s\n", c_programName);

    if (s_config.verbose) {
        vgacon_setLogLevel(VGACON_LOG_LEVEL_DEBUG);
    }

    if (s_config.address == 0) {
        return gus_testAllCards(s_config.ignoreResetFail);
    }

    return (int) gus_testOne(s_config.address, s_config.ignoreResetFail);
}

