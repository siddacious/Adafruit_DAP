/*
 * Copyright (c) 2013-2017, Alex Taradov <alex@taradov.com>, Adafruit <info@adafruit.com>
 * All rights reserved.
 *
 * This is mostly a re-mix of Alex Taradovs excellent Free-DAP code - we just put both halves into one library and wrapped it up in Arduino compatibility
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*- Includes ----------------------------------------------------------------*/
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Adafruit_DAP.h"
#include "dap.h"

/*- Definitions -------------------------------------------------------------*/
#define NRF5X_FLASH_START            0
#define NRF5X_FLASH_ROW_SIZE         256
#define NRF5X_FLASH_PAGE_SIZE        64

#define NRF5X_DHCSR                  0xe000edf0
#define NRF5X_DEMCR                  0xe000edfc
#define NRF5X_AIRCR                  0xe000ed0c

#define NRF5X_FICR_CODEPAGESIZE      0x10000010     // Code memory page size
#define NRF5X_FICR_CODESIZE          0x10000014     // Code size (in pages)
#define NRF5X_FICR_HWID              0x10000100     // Part Code
#define NRF5X_FICR_CHIPVARIANT       0x10000104     // Part Variant
#define NRF5X_FICR_PACKAGEID         0x10000108     // Package Options
#define NRF5X_FICR_SRAM              0x1000010C     // RAM Variant
#define NRF5X_FICR_FLASHSIZE         0x10000110     // Flash Variant

// TODO: Change these from SAMD to nRF5x compatible registers!
#define NRF5X_DSU_CTRL_STATUS        0x41002100

#define NRF5X_NVMCTRL_CTRLA          0x41004000
#define NRF5X_NVMCTRL_CTRLB          0x41004004
#define NRF5X_NVMCTRL_PARAM          0x41004008
#define NRF5X_NVMCTRL_INTFLAG        0x41004014
#define NRF5X_NVMCTRL_STATUS         0x41004018
#define NRF5X_NVMCTRL_ADDR           0x4100401c

#define NRF5X_NVMCTRL_CMD_ER         0xa502
#define NRF5X_NVMCTRL_CMD_WP         0xa504
#define NRF5X_NVMCTRL_CMD_EAR        0xa505
#define NRF5X_NVMCTRL_CMD_WAP        0xa506
#define NRF5X_NVMCTRL_CMD_WL         0xa50f
#define NRF5X_NVMCTRL_CMD_UR         0xa541
#define NRF5X_NVMCTRL_CMD_PBC        0xa544
#define NRF5X_NVMCTRL_CMD_SSB        0xa545

#define NRF5X_USER_ROW_ADDR          0x00804000
#define NRF5X_USER_ROW_SIZE          256

//-----------------------------------------------------------------------------
bool Adafruit_DAP_nRF5x::select(uint32_t *found_id)
{
  uint32_t hwid;
  uint32_t chipvariant;
  uint32_t codepagesize;
  uint32_t codesize;
  uint32_t sram;

  // Stop the core
  dap_write_word(NRF5X_DHCSR, 0xa05f0003);
  dap_write_word(NRF5X_DEMCR, 0x00000001);
  dap_write_word(NRF5X_AIRCR, 0x05fa0004);

  hwid = dap_read_word(NRF5X_FICR_HWID);

  *found_id = hwid;

  if (hwid == 0x52832)
  {
      // Read other relevant registers
      chipvariant = dap_read_word(NRF5X_FICR_CHIPVARIANT);
      codepagesize = dap_read_word(NRF5X_FICR_CODEPAGESIZE);
      codesize = dap_read_word(NRF5X_FICR_CODESIZE);
      //sram = dap_read_word(NRF5X_FICR_SRAM);

      // Assign device details to target_device
      target_device.dsu_did = hwid;
      target_device.flash_size = codepagesize * codesize;
      target_device.n_pages = codesize;
      switch (chipvariant)
      {
          case 0x41414141:
            target_device.name = "nRF52832_AAAA";
            break;
          case 0x41414142:
            target_device.name = "nRF52832_AAAB";
            break;
          case 0x41414241:
            target_device.name = "nRF52832_AABA";
            break;
          case 0x41414242:
            target_device.name = "nRF52832_AABB";
            break;
          case 0x41414230:
            target_device.name = "nRF52832_AAB0";
            break;
          default:
            target_device.name = "nRF52832_????";
            break;
      }
  }
  else
  {
      // No matching device ID found
      return false;
  }

  return true;
}

//-----------------------------------------------------------------------------
void Adafruit_DAP_nRF5x::deselect(void)
{
  dap_write_word(NRF5X_DEMCR, 0x00000000);
  dap_write_word(NRF5X_AIRCR, 0x05fa0004);
}

//-----------------------------------------------------------------------------
void Adafruit_DAP_nRF5x::erase(void)
{
  dap_write_word(NRF5X_DSU_CTRL_STATUS, 0x00001f00); // Clear flags
  dap_write_word(NRF5X_DSU_CTRL_STATUS, 0x00000010); // Chip erase
  delay(100);
  while (0 == (dap_read_word(NRF5X_DSU_CTRL_STATUS) & 0x00000100));
}

//-----------------------------------------------------------------------------
void Adafruit_DAP_nRF5x::lock(void)
{
  dap_write_word(NRF5X_NVMCTRL_CTRLA, NRF5X_NVMCTRL_CMD_SSB); // Set Security Bit
}

//-----------------------------------------------------------------------------
uint32_t Adafruit_DAP_nRF5x::program_start(uint32_t offset)
{

  if (dap_read_word(NRF5X_DSU_CTRL_STATUS) & 0x00010000)
    perror_exit("device is locked, perform a chip erase before programming");

  dap_write_word(NRF5X_NVMCTRL_CTRLB, 0); // Enable automatic write

  //TODO: comvert to slow/fast clock mode
  dap_setup_clock(0);

  return NRF5X_FLASH_START + offset;
}

void Adafruit_DAP_nRF5x::programBlock(uint32_t addr, uint8_t *buf)
{
    dap_write_word(NRF5X_NVMCTRL_ADDR, addr >> 1);

    dap_write_word(NRF5X_NVMCTRL_CTRLA, NRF5X_NVMCTRL_CMD_UR); // Unlock Region
    while (0 == (dap_read_word(NRF5X_NVMCTRL_INTFLAG) & 1));

    dap_write_word(NRF5X_NVMCTRL_CTRLA, NRF5X_NVMCTRL_CMD_ER); // Erase Row
    while (0 == (dap_read_word(NRF5X_NVMCTRL_INTFLAG) & 1));
    dap_write_block(addr, buf, NRF5X_FLASH_ROW_SIZE);
}

//-----------------------------------------------------------------------------
void Adafruit_DAP_nRF5x::readBlock(uint32_t addr, uint8_t *buf)
{
  if (dap_read_word(NRF5X_DSU_CTRL_STATUS) & 0x00010000)
    perror_exit("device is locked, unable to read");

  dap_read_block(addr, buf, NRF5X_FLASH_ROW_SIZE);
}

void Adafruit_DAP_nRF5x::fuseRead(){
  uint8_t buf[NRF5X_USER_ROW_SIZE];
  dap_read_block(NRF5X_USER_ROW_ADDR, buf, NRF5X_USER_ROW_SIZE);

  uint64_t fuses = ((uint64_t)buf[7] << 56) |
          ((uint64_t)buf[6] << 48) |
          ((uint64_t)buf[5] << 40) |
          ((uint64_t)buf[4] << 32) |
          ((uint64_t)buf[3] << 24) |
          ((uint64_t)buf[2] << 16) |
          ((uint64_t)buf[1] << 8) |
          (uint64_t)buf[0];

  _USER_ROW.set(fuses);
}

void Adafruit_DAP_nRF5x::fuseWrite()
{
  uint64_t fuses = _USER_ROW.get();
  uint8_t buf[NRF5X_USER_ROW_SIZE] = {(uint8_t)fuses,
      (uint8_t)(fuses >> 8),
      (uint8_t)(fuses >> 16),
      (uint8_t)(fuses >> 24),
      (uint8_t)(fuses >> 32),
      (uint8_t)(fuses >> 40),
      (uint8_t)(fuses >> 48),
      (uint8_t)(fuses >> 56)
    };

  dap_write_word(NRF5X_NVMCTRL_CTRLB, 0);
  dap_write_word(NRF5X_NVMCTRL_ADDR, NRF5X_USER_ROW_ADDR >> 1);
  dap_write_word(NRF5X_NVMCTRL_CTRLA, NRF5X_NVMCTRL_CMD_EAR);
  while (0 == (dap_read_word(NRF5X_NVMCTRL_INTFLAG) & 1));

  dap_write_block(NRF5X_USER_ROW_ADDR, buf, NRF5X_USER_ROW_SIZE);
}
