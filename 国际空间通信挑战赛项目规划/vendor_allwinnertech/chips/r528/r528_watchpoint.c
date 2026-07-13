/****************************************************************************
 * chip/r528/r528_watchpoint.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/nuttx.h>
#include <nuttx/sched.h>
#include <syslog.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

#define arm_isb(n) __asm__ __volatile__("isb " #n : : : "memory")
#define ARM_ISB() arm_isb(15)

/* OSLSR os lock model bits */
#define ARM_OSLSR_OSLM0 (1 << 0)

/* opcode2 numbers for the co-processor instructions. */
#define ARM_OP2_BVR 4
#define ARM_OP2_BCR 5
#define ARM_OP2_WVR 6
#define ARM_OP2_WCR 7

/* Base register numbers for the debug registers. */
#define ARM_BASE_BVR 64
#define ARM_BASE_BCR 80
#define ARM_BASE_WVR 96
#define ARM_BASE_WCR 112

/* Debug architecture numbers. */
#define ARM_DEBUG_ARCH_RESERVED 0 /* In case of ptrace ABI updates. */
#define ARM_DEBUG_ARCH_V6 1
#define ARM_DEBUG_ARCH_V6_1 2
#define ARM_DEBUG_ARCH_V7_ECP14 3
#define ARM_DEBUG_ARCH_V7_MM 4
#define ARM_DEBUG_ARCH_V7_1 5
#define ARM_DEBUG_ARCH_V8 6
#define ARM_DEBUG_ARCH_V8_1 7
#define ARM_DEBUG_ARCH_V8_2 8
#define ARM_DEBUG_ARCH_V8_4 9

/* DSCR monitor/halting bits. */
#define ARM_DSCR_HDBGEN (1 << 14)
#define ARM_DSCR_MDBGEN (1 << 15)

/* Accessor macros for the debug registers. */
#define ARM_DBG_READ(N, M, OP2, VAL)                                           \
  do {                                                                         \
    asm volatile("mrc p14, 0, %0, " #N "," #M ", " #OP2 : "=r"(VAL));          \
  } while (0)

#define ARM_DBG_WRITE(N, M, OP2, VAL)                                          \
  do {                                                                         \
    asm volatile("mcr p14, 0, %0, " #N "," #M ", " #OP2 : : "r"(VAL));         \
  } while (0)

#define CPUID_ID 0
#define CPUID_CACHETYPE 1
#define CPUID_TCM 2
#define CPUID_TLBTYPE 3
#define CPUID_MPUIR 4
#define CPUID_MPIDR 5
#define CPUID_REVIDR 6
#define STRINGIFY(s) #s

#define read_cpuid(reg)                                                        \
  ({                                                                           \
    unsigned int __val;                                                        \
    asm("mrc	p15, 0, %0, c0, c0, " STRINGIFY(reg) : "=r"(__val) : : "cc");  \
    __val;                                                                     \
  })

#define READ_WB_REG_CASE(OP2, M, VAL)                                          \
  case ((OP2 << 4) + M):                                                       \
    ARM_DBG_READ(c0, c##M, OP2, VAL);                                          \
    break

#define WRITE_WB_REG_CASE(OP2, M, VAL)                                         \
  case ((OP2 << 4) + M):                                                       \
    ARM_DBG_WRITE(c0, c##M, OP2, VAL);                                         \
    break

#define GEN_READ_WB_REG_CASES(OP2, VAL)                                        \
  READ_WB_REG_CASE(OP2, 0, VAL);                                               \
  READ_WB_REG_CASE(OP2, 1, VAL);                                               \
  READ_WB_REG_CASE(OP2, 2, VAL);                                               \
  READ_WB_REG_CASE(OP2, 3, VAL);                                               \
  READ_WB_REG_CASE(OP2, 4, VAL);                                               \
  READ_WB_REG_CASE(OP2, 5, VAL);                                               \
  READ_WB_REG_CASE(OP2, 6, VAL);                                               \
  READ_WB_REG_CASE(OP2, 7, VAL);                                               \
  READ_WB_REG_CASE(OP2, 8, VAL);                                               \
  READ_WB_REG_CASE(OP2, 9, VAL);                                               \
  READ_WB_REG_CASE(OP2, 10, VAL);                                              \
  READ_WB_REG_CASE(OP2, 11, VAL);                                              \
  READ_WB_REG_CASE(OP2, 12, VAL);                                              \
  READ_WB_REG_CASE(OP2, 13, VAL);                                              \
  READ_WB_REG_CASE(OP2, 14, VAL);                                              \
  READ_WB_REG_CASE(OP2, 15, VAL)

#define GEN_WRITE_WB_REG_CASES(OP2, VAL)                                       \
  WRITE_WB_REG_CASE(OP2, 0, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 1, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 2, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 3, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 4, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 5, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 6, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 7, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 8, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 9, VAL);                                              \
  WRITE_WB_REG_CASE(OP2, 10, VAL);                                             \
  WRITE_WB_REG_CASE(OP2, 11, VAL);                                             \
  WRITE_WB_REG_CASE(OP2, 12, VAL);                                             \
  WRITE_WB_REG_CASE(OP2, 13, VAL);                                             \
  WRITE_WB_REG_CASE(OP2, 14, VAL);                                             \
  WRITE_WB_REG_CASE(OP2, 15, VAL)

static uint32_t read_wb_reg(int n) {
  uint32_t val = 0;

  switch (n) {
    GEN_READ_WB_REG_CASES(ARM_OP2_BVR, val);
    GEN_READ_WB_REG_CASES(ARM_OP2_BCR, val);
    GEN_READ_WB_REG_CASES(ARM_OP2_WVR, val);
    GEN_READ_WB_REG_CASES(ARM_OP2_WCR, val);
  default:
    syslog(0, "attempt to read from unknown breakpoint register %d\n", n);
  }

  return val;
}

static void write_wb_reg(int n, uint32_t val) {
  switch (n) {
    GEN_WRITE_WB_REG_CASES(ARM_OP2_BVR, val);
    GEN_WRITE_WB_REG_CASES(ARM_OP2_BCR, val);
    GEN_WRITE_WB_REG_CASES(ARM_OP2_WVR, val);
    GEN_WRITE_WB_REG_CASES(ARM_OP2_WCR, val);
  default:
    syslog(0, "attempt to write to unknown breakpoint register %d\n", n);
  }

  ARM_ISB();
}

/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant.  Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
static inline unsigned int read_cpuid_id(void) { return read_cpuid(CPUID_ID); }

/* Determine debug architecture. */
static uint8_t get_debug_arch(void) {
  uint32_t didr;

  /* Do we implement the extended CPUID interface? */
  if (((read_cpuid_id() >> 16) & 0xf) != 0xf) {
    syslog(0, "CPUID feature registers not supported. "
              "Assuming v6 debug is present.\n");
    return ARM_DEBUG_ARCH_V6;
  }

  ARM_DBG_READ(c0, c0, 0, didr);
  return (didr >> 16) & 0xf;
}

static int enable_monitor_mode(void) {
  uint32_t dscr;

  ARM_DBG_READ(c0, c1, 0, dscr);

  /* If monitor mode is already enabled, just return. */
  if (dscr & ARM_DSCR_MDBGEN)
    goto out;

  /* Write to the corresponding DSCR. */
  int arch = get_debug_arch();
  switch (arch) {
  case ARM_DEBUG_ARCH_V6:
  case ARM_DEBUG_ARCH_V6_1:
    ARM_DBG_WRITE(c0, c1, 0, (dscr | ARM_DSCR_MDBGEN));
    break;
  case ARM_DEBUG_ARCH_V7_ECP14:
  case ARM_DEBUG_ARCH_V7_1:
  case ARM_DEBUG_ARCH_V8:
  case ARM_DEBUG_ARCH_V8_1:
  case ARM_DEBUG_ARCH_V8_2:
  case ARM_DEBUG_ARCH_V8_4:
    ARM_DBG_WRITE(c0, c2, 2, (dscr | ARM_DSCR_MDBGEN));
    ARM_ISB();
    break;
  default:
    return -ENODEV;
  }

  /* Check that the write made it through. */
  ARM_DBG_READ(c0, c1, 0, dscr);
  if (!(dscr & ARM_DSCR_MDBGEN)) {
    syslog(0, "Failed to enable monitor mode.\n");
    return -EPERM;
  }

out:
  return 0;
}

int arm_install_hw_watchpoint(int i, uint32_t addr, uint32_t mask_bits,
                              bool enable) {
  uint32_t ctrl =
      (enable ? 1u : 0) << 0 | // E = 1 enable
      3u << 1 |                // PAC = 3 0x11
      2u << 3 |                // LSC = 3  load and store
      0xff << 5 |              // BAS = 0xff
      0u << 13 |               // HMC = 0
      0u << 14 |               // SSC = 0
      0u << 16 |               // LBN = 0
      0u << 20 |               // WT = 1
      (mask_bits) << 24; // MSK = 0 mask 18 address bit valid addr 0x0~0x1ffff

  uint32_t check_value;

  /* Setup the address register. */
  write_wb_reg(ARM_BASE_WVR + i, addr);
  check_value = read_wb_reg(ARM_BASE_WVR + i);
  if (check_value != addr) {
    syslog(0,
           "fail to set WVR[%d] addr:0x%" PRIx32 " check_value:0x%" PRIx32 "\n",
           i, addr, check_value);
    return -1;
  }

  /* Setup the control register. */
  write_wb_reg(ARM_BASE_WCR + i, ctrl);
  check_value = read_wb_reg(ARM_BASE_WCR + i);
  if (check_value != ctrl) {
    syslog(0,
           "fail to set WCR[%d] ctrl:0x%" PRIx32 " check_value:0x%" PRIx32 "\n",
           i, ctrl, check_value);
    return -1;
  }

  return 0;
}

int arm_install_hw_breakpoint(int i, uint32_t addr, bool enable) {
  uint32_t ctrl = (enable ? 1u : 0) << 0 | // E = 1 enable
                  3u << 1 |                // PAC = 3 0x11 all
                  0xf << 5 |               // BAS = 15
                  0u << 13 |               // HMC = 0
                  0u << 14 |               // SSC = 0
                  0u << 16 |               // LBN = 0
                  0u << 20 |               // WT = 1
                  0 << 24; // R528 doesn't implement this feature.

  uint32_t check_value;
  /* Setup the address register. */
  write_wb_reg(ARM_BASE_BVR + i, addr);
  check_value = read_wb_reg(ARM_BASE_BVR + i);
  if (check_value != addr) {
    syslog(0,
           "fail to set BCR[%d] addr:0x%" PRIx32 " check_value:0x%" PRIx32 "\n",
           i, addr, check_value);
    return -1;
  }

  /* Setup the control register. */
  write_wb_reg(ARM_BASE_BCR + i, ctrl);
  check_value = read_wb_reg(ARM_BASE_BCR + i);
  if (check_value != ctrl) {
    syslog(0,
           "fail to set BCR[%d] ctrl:0x%" PRIx32 " check_value:0x%" PRIx32 "\n",
           i, ctrl, check_value);
    return -1;
  }

  return 0;
}

int r528_protect_brom(void) {
  int value;
  ARM_DBG_WRITE(c1, c0, 4, 0); /*Write OS Lock Access Register n */

  /* arm_install_hw_watchpoint(0, 0, 17, true); */
  arm_install_hw_watchpoint(0, 0x040210b8, 2, true);

  /* Break point can only match specified address, we setup 8 addresses here. */
  arm_install_hw_breakpoint(0, 0, true);
  arm_install_hw_breakpoint(1, 0x48, true);
  arm_install_hw_breakpoint(2, 0x68, true);
  arm_install_hw_breakpoint(3, 0xa0, true);
  arm_install_hw_breakpoint(4, 0xd4, true);
  arm_install_hw_breakpoint(5, 0xf0, true);
  enable_monitor_mode();

  value = read_wb_reg(ARM_BASE_BCR);
  syslog(0, "ARM_BASE_BCR: %x\n", value);
  value = read_wb_reg(ARM_BASE_WCR);
  syslog(0, "ARM_BASE_WCR: %x\n", value);
  return 0;
}
