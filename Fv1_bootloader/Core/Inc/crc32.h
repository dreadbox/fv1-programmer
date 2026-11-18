#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

/* Initialize the hardware CRC peripheral. */
void CRC32_Init(void);

/* Calculate CRC32 over a region of flash. len must be a multiple of 4. */
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len);

#endif /* CRC32_H */
