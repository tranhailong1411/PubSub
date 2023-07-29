#ifndef LORA_MAC_GW_CFG_H
#define LORA_MAC_GW_CFG_H

#ifdef __cplusplus
extern "C" {
#endif
	
#include "LoRaMac_GW_Types.h"

#define LORAMAC_VERSION                LORAMAC_VERSION_2_DATA_COLLECTING
	
#define NETWORK_ADDR                   ((255<<24) | (255<<16) | (255<<8) | 0)

#define DEV_ADDR                       ((NETWORK_ADDR) | 0)

#ifdef __cplusplus
}
#endif

#endif /* LORA_MAC_GW_CFG_H */
