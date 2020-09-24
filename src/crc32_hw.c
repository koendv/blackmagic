#include "general.h"

#ifdef PLATFORM_HAS_HARDWARE_CRC

#include STM32_HAL_H
#include "crc32.h"
#include "exception.h"

/* crc for gdb 'compare-sections' */

/* Use hardware crc */

void generic_crc32_init(void) {
  __HAL_RCC_CRC_CLK_ENABLE();
}

/* calculate crc of target memory */
uint32_t generic_crc32(target *t, uint32_t base, size_t len) {
  size_t read_len = 0;
  uint8_t bytes[128] __attribute__((aligned(4)));
  uint32_t crc = -1;
  CRC_HandleTypeDef hcrc = {
      .Instance = CRC,
      .Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE,
      .Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE,
      .Init.CRCLength = CRC_POLYLENGTH_32B,
      .Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE,
      .Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE,
      .InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES,
  };

  if ((HAL_CRC_DeInit(&hcrc) == HAL_OK) && (HAL_CRC_Init(&hcrc) == HAL_OK)) {
    if (len > 0) {
      read_len = MIN(sizeof(bytes), len);
      if (target_mem_read(t, bytes, base, read_len))
        raise_exception(EXCEPTION_ERROR, "target mem read failed\n");
      crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)bytes, read_len);
      base += read_len;
      len -= read_len;
    }
    while (len > 0) {
      read_len = MIN(sizeof(bytes), len);
      if (target_mem_read(t, bytes, base, read_len))
        raise_exception(EXCEPTION_ERROR, "target mem read failed\n");
      crc = HAL_CRC_Accumulate(&hcrc, (uint32_t *)bytes, read_len);
      base += read_len;
      len -= read_len;
    }
  } else
    raise_exception(EXCEPTION_ERROR, "crc init failed\n");
  return crc;
}

#endif
