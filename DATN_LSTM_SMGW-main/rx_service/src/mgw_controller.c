/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf sprintf fopen fputs */

#include <string.h>     /* memset */
#include <signal.h>     /* sigaction */
#include <time.h>       /* time clock_gettime strftime gmtime clock_nanosleep*/
#include <unistd.h>     /* getopt access */
#include <stdlib.h>     /* atoi */

#include <sys/socket.h> 
#include <arpa/inet.h> 

#include <pthread.h>
#include <sys/time.h> 
#include </usr/include/python3.7/Python.h>

#include "trace.h"
#include "parson.h"
#include "loragw_hal.h"

#include "LoRaMac_GW_Types.h"
#include "Std_Types.h"

#define PORT 23 
// GW IP 7
//#define GW_ADDR ((7<<24) | (201<<16) | (0<<8) | 0)
#define GW_ADDR ((255<<24) | (255<<16) | (0<<8) | 0)
/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define STRINGIFY(x)    #x
#define STR(x)          STRINGIFY(x)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#ifndef VERSION_STRING
  #define VERSION_STRING "undefined"
#endif

#define NB_PKT_MAX      8 /* max number of packets per fetch/send cycle */

#define STATUS_SIZE     200
#define TX_BUFF_SIZE    ((540 * NB_PKT_MAX) + 30 + STATUS_SIZE)

#define TX_RF_CHAIN                 0 /* TX only supported on radio A */
#define DEFAULT_RSSI_OFFSET         0.0
#define DEFAULT_MODULATION          "LORA"
#define DEFAULT_BR_KBPS             50
#define DEFAULT_FDEV_KHZ            25
#define DEFAULT_NOTCH_FREQ          129000U /* 129 kHz */
#define DEFAULT_SX127X_RSSI_OFFSET  -4 /* dB */
/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* network configuration variables */
static uint64_t lgwm = 0; /* Lora gateway MAC address */
char lgwm_str[17];

/* hardware access control and correction */
pthread_mutex_t mx_concent = PTHREAD_MUTEX_INITIALIZER; /* control access to the concentrator */

/* Gateway specificities */
static int8_t antenna_gain = 0;

/* TX capabilities */
static struct lgw_tx_gain_lut_s txlut; /* TX gain table */
static uint32_t tx_freq_min[LGW_RF_CHAIN_NB]; /* lowest frequency supported by TX chain */
static uint32_t tx_freq_max[LGW_RF_CHAIN_NB]; /* highest frequency supported by TX chain */

/* clock and log rx_file and tx_file management */
time_t now_time;

time_t rxfile_start_time;
FILE * rxfile = NULL;
char rxfile_name[64];

time_t txfile_start_time;
FILE * txfile = NULL;
char txfile_name[64];

/* clock and log rotation management */
int log_rotate_interval = 3600; /* by default, rotation every hour */
int time_check = 0; /* variable used to limit the number of calls to time() function */
unsigned long pkt_in_log = 0; /* count the number of packet written in each log file */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

static int parse_SX1301_configuration(const char * conf_file);

static int parse_gateway_configuration(const char * conf_file);

void open_rxlog(void);

void open_txlog(void);

void usage (void);

/* threads */
void thread_up(void);
void thread_down(void);
void thread_valid(void);

static Std_ReturnType LoRaSV_SendACK();

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */
float* extract_data(char* data_receive)
{
    static float temp[8];
    int index = 0;
    
    char * token = strtok(data_receive, ",");
    while( token != NULL ) {
        temp[index] = strtof(token, NULL);
        token = strtok(NULL, ",");
        index++;
    }
    
    return temp;
}

LoRaMacSerializerStatus_t LoRaMac_SerializePacket(LoRaMacMessageData_t* mac_msg)
{
	
	uint16_t bufItr = 0;
	
	/* Calculate buffer size*/
	uint16_t computedBufSize = LORAMAC_MHDR_FIELD_SIZE
                           + LORAMAC_FHDR_DEV_ADD_FIELD_SIZE
                           + LORAMAC_FHDR_F_CTRL_FIELD_SIZE
                           + LORAMAC_FHDR_F_CNT_FIELD_SIZE
	                         + LORAMAC_FHDR_F_LEN_FIELD_SIZE;
	
	if(mac_msg->FRMPayloadSize > 0)
	{
		computedBufSize += LORAMAC_F_PORT_FIELD_SIZE;
	}
	
	computedBufSize += mac_msg->FRMPayloadSize;
	
	/* Check macMsg->BufSize */
	if(mac_msg->BufSize < computedBufSize)
	{
		return LORAMAC_SERIALIZER_ERROR_BUF_SIZE;
	}
	
	mac_msg->Buffer[bufItr++] = mac_msg->MHDR.Value;
	mac_msg->Buffer[bufItr++] = (mac_msg->FHDR.DevAddr) & 0xFF;
	mac_msg->Buffer[bufItr++] = (mac_msg->FHDR.DevAddr >> 8) & 0xFF;
	mac_msg->Buffer[bufItr++] = (mac_msg->FHDR.DevAddr >> 16) & 0xFF;
	mac_msg->Buffer[bufItr++] = (mac_msg->FHDR.DevAddr >> 24) & 0xFF;

	mac_msg->Buffer[bufItr++] = mac_msg->FHDR.FCtrl.Value;
	mac_msg->Buffer[bufItr++] = mac_msg->FHDR.FCnt & 0xFF;
	mac_msg->Buffer[bufItr++] = (mac_msg->FHDR.FCnt >> 8) & 0xFF;
	mac_msg->Buffer[bufItr++] = mac_msg->FHDR.FLen;
	
    

	if(mac_msg->FRMPayloadSize > 0)
	{
		mac_msg->Buffer[bufItr++] = mac_msg->FPort;
	}
	else
	{
        mac_msg->BufSize = bufItr;
		return LORAMAC_SERIALIZER_SUCCESS;
	}
	
	for(uint8_t i = 0; i < mac_msg->FRMPayloadSize; i++)
	{
		mac_msg->Buffer[bufItr++] = mac_msg->FRMPayload[i];
	}

	mac_msg->BufSize = bufItr;

    
	
	return LORAMAC_SERIALIZER_SUCCESS;
}

LoRaMacSerializerStatus_t LoRaMac_SetUpACKMessage
(
  LoRaMacMessageData_t *ack_msg,
  uint32_t dev_addr,
  uint16_t cnt_reply
)
{
	LoRaMacSerializerStatus_t serialize_ret = LORAMAC_SERIALIZER_ERROR;
    uint8_t cmd_len = 0;
    char * cmd;

	ack_msg->BufSize = 255;
	
    ack_msg->FHDR.DevAddr = dev_addr;
	
	ack_msg->FHDR.FCtrl.Bits.Ack = 1;
	
	ack_msg->FHDR.FCnt = cnt_reply;
	
	ack_msg->FHDR.FLen = 0;
	
	ack_msg->FPort = 0;
	
	ack_msg->FRMPayloadSize = cmd_len;

    /* check if there is cmd for this node */
    if(cmd_len != 0)
    {
        ack_msg->MHDR.Bits.MType = FRAME_TYPE_DATA_CONFIRMED;
        strcpy(ack_msg->FRMPayload,cmd);
    }
    else
    {
        ack_msg->MHDR.Bits.MType = FRAME_TYPE_DATA_UNCONFIRMED;
        memset(ack_msg->FRMPayload,'\0',strlen(ack_msg->FRMPayload));
    }
	
	serialize_ret = LoRaMac_SerializePacket(ack_msg);
	printf("%u\r\n",ack_msg->Buffer[3]);
	return serialize_ret;
}

LoRaMacParserStatus_t LoRaMac_ParserData( LoRaMacMessageData_t *mac_msg )
{
	uint16_t bufItr = 0;
	
	mac_msg->MHDR.Value = mac_msg->Buffer[bufItr++];
	mac_msg->FHDR.DevAddr = mac_msg->Buffer[bufItr++];
	mac_msg->FHDR.DevAddr |= ((uint32_t) mac_msg->Buffer[bufItr++] << 8);
	mac_msg->FHDR.DevAddr |= ((uint32_t) mac_msg->Buffer[bufItr++] << 16);
	mac_msg->FHDR.DevAddr |= ((uint32_t) mac_msg->Buffer[bufItr++] << 24);
	
	mac_msg->FHDR.FCtrl.Value = mac_msg->Buffer[bufItr++];
	
	mac_msg->FHDR.FCnt = mac_msg->Buffer[bufItr++];
	mac_msg->FHDR.FCnt |= mac_msg->Buffer[bufItr++] << 8;
	
	mac_msg->FHDR.FLen = mac_msg->Buffer[bufItr++];
	
	mac_msg->FPort = 0;
	mac_msg->FRMPayloadSize = 0;
	
	if((mac_msg->BufSize - bufItr) > 0)
	{
		mac_msg->FPort = mac_msg->Buffer[bufItr++];
		mac_msg->FRMPayloadSize = (mac_msg->BufSize - bufItr);
    if(mac_msg->FRMPayloadSize != mac_msg->FHDR.FLen)
		{
			return LORAMAC_PARSER_FAIL;
		}
		
		for(uint8_t i = 0; i < mac_msg->FRMPayloadSize; i++)
		{
			mac_msg->FRMPayload[i] = mac_msg->Buffer[bufItr++];
		}
	}
	
	return LORAMAC_PARSER_SUCCESS;
}




static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = true;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = true;
    }
    return;
}

static int parse_SX1301_configuration(const char * conf_file) {
    int i;
    char param_name[32]; /* used to generate variable parameter names */
    const char *str; /* used to store string value from JSON object */
    const char conf_obj_name[] = "SX1301_conf";
    JSON_Value *root_val = NULL;
    JSON_Object *conf_obj = NULL;
    JSON_Object *conf_lbt_obj = NULL;
    JSON_Object *conf_lbtchan_obj = NULL;
    JSON_Value *val = NULL;
    JSON_Array *conf_array = NULL;
    struct lgw_conf_board_s boardconf;
    struct lgw_conf_lbt_s lbtconf;
    struct lgw_conf_rxrf_s rfconf;
    struct lgw_conf_rxif_s ifconf;
    uint32_t sf, bw, fdev;

    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        MSG("ERROR: %s is not a valid JSON file\n", conf_file);
        exit(EXIT_FAILURE);
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        MSG("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        return -1;
    } else {
        MSG("INFO: %s does contain a JSON object named %s, parsing SX1301 parameters\n", conf_file, conf_obj_name);
    }

    /* set board configuration */
    memset(&boardconf, 0, sizeof boardconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "lorawan_public"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONBoolean) {
        boardconf.lorawan_public = (bool)json_value_get_boolean(val);
    } else {
        MSG("WARNING: Data type for lorawan_public seems wrong, please check\n");
        boardconf.lorawan_public = false;
    }
    val = json_object_get_value(conf_obj, "clksrc"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONNumber) {
        boardconf.clksrc = (uint8_t)json_value_get_number(val);
    } else {
        MSG("WARNING: Data type for clksrc seems wrong, please check\n");
        boardconf.clksrc = 0;
    }
    MSG("INFO: lorawan_public %d, clksrc %d\n", boardconf.lorawan_public, boardconf.clksrc);
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_board_setconf(boardconf) != LGW_HAL_SUCCESS) {
        MSG("ERROR: Failed to configure board\n");
        return -1;
    }

    /* set LBT configuration */
    memset(&lbtconf, 0, sizeof lbtconf); /* initialize configuration structure */
    conf_lbt_obj = json_object_get_object(conf_obj, "lbt_cfg"); /* fetch value (if possible) */
    if (conf_lbt_obj == NULL) {
        MSG("INFO: no configuration for LBT\n");
    } else {
        val = json_object_get_value(conf_lbt_obj, "enable"); /* fetch value (if possible) */
        if (json_value_get_type(val) == JSONBoolean) {
            lbtconf.enable = (bool)json_value_get_boolean(val);
        } else {
            MSG("WARNING: Data type for lbt_cfg.enable seems wrong, please check\n");
            lbtconf.enable = false;
        }
        if (lbtconf.enable == true) {
            val = json_object_get_value(conf_lbt_obj, "rssi_target"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONNumber) {
                lbtconf.rssi_target = (int8_t)json_value_get_number(val);
            } else {
                MSG("WARNING: Data type for lbt_cfg.rssi_target seems wrong, please check\n");
                lbtconf.rssi_target = 0;
            }
            val = json_object_get_value(conf_lbt_obj, "sx127x_rssi_offset"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONNumber) {
                lbtconf.rssi_offset = (int8_t)json_value_get_number(val);
            } else {
                MSG("WARNING: Data type for lbt_cfg.sx127x_rssi_offset seems wrong, please check\n");
                lbtconf.rssi_offset = 0;
            }
            /* set LBT channels configuration */
            conf_array = json_object_get_array(conf_lbt_obj, "chan_cfg");
            if (conf_array != NULL) {
                lbtconf.nb_channel = json_array_get_count( conf_array );
                MSG("INFO: %u LBT channels configured\n", lbtconf.nb_channel);
            }
            for (i = 0; i < (int)lbtconf.nb_channel; i++) {
                /* Sanity check */
                if (i >= LBT_CHANNEL_FREQ_NB)
                {
                    MSG("ERROR: LBT channel %d not supported, skip it\n", i );
                    break;
                }
                /* Get LBT channel configuration object from array */
                conf_lbtchan_obj = json_array_get_object(conf_array, i);

                /* Channel frequency */
                val = json_object_dotget_value(conf_lbtchan_obj, "freq_hz"); /* fetch value (if possible) */
                if (json_value_get_type(val) == JSONNumber) {
                    lbtconf.channels[i].freq_hz = (uint32_t)json_value_get_number(val);
                } else {
                    MSG("WARNING: Data type for lbt_cfg.channels[%d].freq_hz seems wrong, please check\n", i);
                    lbtconf.channels[i].freq_hz = 0;
                }

                /* Channel scan time */
                val = json_object_dotget_value(conf_lbtchan_obj, "scan_time_us"); /* fetch value (if possible) */
                if (json_value_get_type(val) == JSONNumber) {
                    lbtconf.channels[i].scan_time_us = (uint16_t)json_value_get_number(val);
                } else {
                    MSG("WARNING: Data type for lbt_cfg.channels[%d].scan_time_us seems wrong, please check\n", i);
                    lbtconf.channels[i].scan_time_us = 0;
                }
            }

            /* all parameters parsed, submitting configuration to the HAL */
            if (lgw_lbt_setconf(lbtconf) != LGW_HAL_SUCCESS) {
                MSG("ERROR: Failed to configure LBT\n");
                return -1;
            }
        } else {
            MSG("INFO: LBT is disabled\n");
        }
    }

    /* set antenna gain configuration */
    val = json_object_get_value(conf_obj, "antenna_gain"); /* fetch value (if possible) */
    if (val != NULL) {
        if (json_value_get_type(val) == JSONNumber) {
            antenna_gain = (int8_t)json_value_get_number(val);
        } else {
            MSG("WARNING: Data type for antenna_gain seems wrong, please check\n");
            antenna_gain = 0;
        }
    }
    MSG("INFO: antenna_gain %d dBi\n", antenna_gain);

    /* set configuration for tx gains */
    memset(&txlut, 0, sizeof txlut); /* initialize configuration structure */
    for (i = 0; i < TX_GAIN_LUT_SIZE_MAX; i++) {
        snprintf(param_name, sizeof param_name, "tx_lut_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG("INFO: no configuration for tx gain lut %i\n", i);
            continue;
        }
        txlut.size++; /* update TX LUT size based on JSON object found in configuration file */
        /* there is an object to configure that TX gain index, let's parse it */
        snprintf(param_name, sizeof param_name, "tx_lut_%i.pa_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].pa_gain = (uint8_t)json_value_get_number(val);
        } else {
            MSG("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].pa_gain = 0;
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.dac_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].dac_gain = (uint8_t)json_value_get_number(val);
        } else {
            txlut.lut[i].dac_gain = 3; /* This is the only dac_gain supported for now */
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.dig_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].dig_gain = (uint8_t)json_value_get_number(val);
        } else {
            MSG("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].dig_gain = 0;
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.mix_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].mix_gain = (uint8_t)json_value_get_number(val);
        } else {
            MSG("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].mix_gain = 0;
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.rf_power", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].rf_power = (int8_t)json_value_get_number(val);
        } else {
            MSG("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].rf_power = 0;
        }
    }
    /* all parameters parsed, submitting configuration to the HAL */
    if (txlut.size > 0) {
        MSG("INFO: Configuring TX LUT with %u indexes\n", txlut.size);
        if (lgw_txgain_setconf(&txlut) != LGW_HAL_SUCCESS) {
            MSG("ERROR: Failed to configure concentrator TX Gain LUT\n");
            return -1;
        }
    } else {
        MSG("WARNING: No TX gain LUT defined\n");
    }

    /* set configuration for RF chains */
    for (i = 0; i < LGW_RF_CHAIN_NB; ++i) {
        memset(&rfconf, 0, sizeof rfconf); /* initialize configuration structure */
        snprintf(param_name, sizeof param_name, "radio_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG("INFO: no configuration for radio %i\n", i);
            continue;
        }
        /* there is an object to configure that radio, let's parse it */
        snprintf(param_name, sizeof param_name, "radio_%i.enable", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            rfconf.enable = (bool)json_value_get_boolean(val);
        } else {
            rfconf.enable = false;
        }
        if (rfconf.enable == false) { /* radio disabled, nothing else to parse */
            MSG("INFO: radio %i disabled\n", i);
        } else  { /* radio enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "radio_%i.freq", i);
            rfconf.freq_hz = (uint32_t)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_offset", i);
            rfconf.rssi_offset = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.type", i);
            str = json_object_dotget_string(conf_obj, param_name);
            if (!strncmp(str, "SX1255", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1255;
            } else if (!strncmp(str, "SX1257", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1257;
            } else {
                MSG("WARNING: invalid radio type: %s (should be SX1255 or SX1257)\n", str);
            }
            snprintf(param_name, sizeof param_name, "radio_%i.tx_enable", i);
            val = json_object_dotget_value(conf_obj, param_name);
            if (json_value_get_type(val) == JSONBoolean) {
                rfconf.tx_enable = (bool)json_value_get_boolean(val);
                if (rfconf.tx_enable == true) {
                    /* tx is enabled on this rf chain, we need its frequency range */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_freq_min", i);
                    tx_freq_min[i] = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_freq_max", i);
                    tx_freq_max[i] = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                    if ((tx_freq_min[i] == 0) || (tx_freq_max[i] == 0)) {
                        MSG("WARNING: no frequency range specified for TX rf chain %d\n", i);
                    }
                    /* ... and the notch filter frequency to be set */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_notch_freq", i);
                    rfconf.tx_notch_freq = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                }
            } else {
                rfconf.tx_enable = false;
            }
            MSG("INFO: radio %i enabled (type %s), center frequency %u, RSSI offset %f, tx enabled %d, tx_notch_freq %u\n", i, str, rfconf.freq_hz, rfconf.rssi_offset, rfconf.tx_enable, rfconf.tx_notch_freq);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxrf_setconf(i, rfconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for radio %i\n", i);
            return -1;
        }
    }

    /* set configuration for Lora multi-SF channels (bandwidth cannot be set) */
    for (i = 0; i < LGW_MULTI_NB; ++i) {
        memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
        snprintf(param_name, sizeof param_name, "chan_multiSF_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG("INFO: no configuration for Lora multi-SF channel %i\n", i);
            continue;
        }
        /* there is an object to configure that Lora multi-SF channel, let's parse it */
        snprintf(param_name, sizeof param_name, "chan_multiSF_%i.enable", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) { /* Lora multi-SF channel disabled, nothing else to parse */
            MSG("INFO: Lora multi-SF channel %i disabled\n", i);
        } else  { /* Lora multi-SF channel enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "chan_multiSF_%i.radio", i);
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "chan_multiSF_%i.if", i);
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, param_name);
            // TODO: handle individual SF enabling and disabling (spread_factor)
            MSG("INFO: Lora multi-SF channel %i>  radio %i, IF %i Hz, 125 kHz bw, SF 7 to 12\n", i, ifconf.rf_chain, ifconf.freq_hz);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxif_setconf(i, ifconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for Lora multi-SF channel %i\n", i);
            return -1;
        }
    }

    /* set configuration for Lora standard channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "chan_Lora_std"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        MSG("INFO: no configuration for Lora standard channel\n");
    } else {
        val = json_object_dotget_value(conf_obj, "chan_Lora_std.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            MSG("INFO: Lora standard channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.if");
            bw = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.bandwidth");
            switch(bw) {
                case 500000: ifconf.bandwidth = BW_500KHZ; break;
                case 250000: ifconf.bandwidth = BW_250KHZ; break;
                case 125000: ifconf.bandwidth = BW_125KHZ; break;
                default: ifconf.bandwidth = BW_UNDEFINED;
            }
            sf = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.spread_factor");
            switch(sf) {
                case  7: ifconf.datarate = DR_LORA_SF7;  break;
                case  8: ifconf.datarate = DR_LORA_SF8;  break;
                case  9: ifconf.datarate = DR_LORA_SF9;  break;
                case 10: ifconf.datarate = DR_LORA_SF10; break;
                case 11: ifconf.datarate = DR_LORA_SF11; break;
                case 12: ifconf.datarate = DR_LORA_SF12; break;
                default: ifconf.datarate = DR_UNDEFINED;
            }
            MSG("INFO: Lora std channel> radio %i, IF %i Hz, %u Hz bw, SF %u\n", ifconf.rf_chain, ifconf.freq_hz, bw, sf);
        }
        if (lgw_rxif_setconf(8, ifconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for Lora standard channel\n");
            return -1;
        }
    }

    /* set configuration for FSK channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "chan_FSK"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        MSG("INFO: no configuration for FSK channel\n");
    } else {
        val = json_object_dotget_value(conf_obj, "chan_FSK.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            MSG("INFO: FSK channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, "chan_FSK.if");
            bw = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.bandwidth");
            fdev = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.freq_deviation");
            ifconf.datarate = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.datarate");

            /* if chan_FSK.bandwidth is set, it has priority over chan_FSK.freq_deviation */
            if ((bw == 0) && (fdev != 0)) {
                bw = 2 * fdev + ifconf.datarate;
            }
            if      (bw == 0)      ifconf.bandwidth = BW_UNDEFINED;
            else if (bw <= 7800)   ifconf.bandwidth = BW_7K8HZ;
            else if (bw <= 15600)  ifconf.bandwidth = BW_15K6HZ;
            else if (bw <= 31200)  ifconf.bandwidth = BW_31K2HZ;
            else if (bw <= 62500)  ifconf.bandwidth = BW_62K5HZ;
            else if (bw <= 125000) ifconf.bandwidth = BW_125KHZ;
            else if (bw <= 250000) ifconf.bandwidth = BW_250KHZ;
            else if (bw <= 500000) ifconf.bandwidth = BW_500KHZ;
            else ifconf.bandwidth = BW_UNDEFINED;

            MSG("INFO: FSK channel> radio %i, IF %i Hz, %u Hz bw, %u bps datarate\n", ifconf.rf_chain, ifconf.freq_hz, bw, ifconf.datarate);
        }
        if (lgw_rxif_setconf(9, ifconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for FSK channel\n");
            return -1;
        }
    }
    json_value_free(root_val);

    return 0;
}

int parse_gateway_configuration(const char * conf_file) {
	const char conf_obj_name[] = "gateway_conf";
    JSON_Value *root_val;
    JSON_Object *conf_obj = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */
    unsigned long long ull = 0;

    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        MSG("ERROR: %s is not a valid JSON file\n", conf_file);
        exit(EXIT_FAILURE);
    }

	/* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        MSG("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        return -1;
    } else {
        MSG("INFO: %s does contain a JSON object named %s, parsing gateway parameters\n", conf_file, conf_obj_name);
    }

    /* gateway unique identifier (aka MAC address) (optional) */
    str = json_object_get_string(conf_obj, "gateway_ID");
    if (str != NULL) {
        sscanf(str, "%llx", &ull);
        lgwm = ull;
        MSG("INFO: gateway MAC address is configured to %016llX\n", ull);
    }
}

void open_rxlog(void) {
    int i;
    char iso_date[20];

    strftime(iso_date,ARRAY_SIZE(iso_date),"%Y%m%dT%H%M%SZ",gmtime(&now_time)); /* format yyyymmddThhmmssZ */
    rxfile_start_time = now_time; /* keep track of when the log was started, for log rotation */

    sprintf(rxfile_name, "/opt/mgw_BBAI/apps/TCP_PF/input/dataUp.csv");
    rxfile = fopen(rxfile_name, "a"); /* create log file, append if file already exist */
    if (rxfile == NULL) {
        MSG("ERROR: impossible to create log file %s\n", rxfile_name);
        exit(EXIT_FAILURE);
    }

    
    //i = fprintf(rxfile, "\"gateway ID\",\"node MAC\",\"UTC timestamp\",\"us count\",\"frequency\",\"RF chain\",\"RX chain\",\"status\",\"size\",\"modulation\",\"bandwidth\",\"datarate\",\"coderate\",\"RSSI\",\"SNR\",\"payload\"\n");
    i = fprintf(rxfile, "Starting...\n");
    if (i < 0) {
        MSG("ERROR: impossible to write to rxfile %s\n", rxfile_name);
        exit(EXIT_FAILURE);
    }

    MSG("INFO: Now writing to rxfile %s\n", rxfile_name);
    return;
}

void open_txlog(void){
    int i;
    char iso_date[20];

    strftime(iso_date,ARRAY_SIZE(iso_date),"%Y%m%dT%H%M%SZ",gmtime(&now_time)); /* format yyyymmddThhmmssZ */
    txfile_start_time = now_time; /* keep track of when the log was started, for log rotation */

    sprintf(txfile_name, "/opt/mgw_BBAI/apps/TCP_PF/output/dataDown.csv");
    txfile = fopen(txfile_name, "a"); /* create log file, append if file already exist */
    if (txfile == NULL) {
        MSG("ERROR: impossible to open tx file %s\n", txfile_name);
        exit(EXIT_FAILURE);
    }

    // MSG("INFO: Now reading from txfile %s\n", txfile_name);
    return;
}

/* describe command line options */
void usage(void) {
    printf("*** Library version information ***\n%s\n\n", lgw_version_info());
    printf( "Available options:\n");
    printf( " -h print this help\n");
    printf( " -r <int> rotate log file every N seconds (-1 disable log rotation)\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

clock_t time_now;
double time_taken;

int main(int argc, char **argv)
{
    struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
    int i; /* loop variable and temporary variable for return value */
    int x;

    /* configuration file related */
    char *global_cfg_path= "global_conf.json"; /* contain global (typ. network-wide) configuration */
    char *local_cfg_path = "local_conf.json"; /* contain node specific configuration, overwrite global parameters for parameters that are defined in both */
    char *debug_cfg_path = "debug_conf.json"; /* if present, all other configuration files are ignored */
    
    /* threads */
    pthread_t thrid_up;
    pthread_t thrid_down;

/* configuration files management */
    if (access(debug_cfg_path, R_OK) == 0) {
    /* if there is a debug conf, parse only the debug conf */
        MSG("INFO: found debug configuration file %s, other configuration files will be ignored\n", debug_cfg_path);
        parse_SX1301_configuration(debug_cfg_path);
        parse_gateway_configuration(debug_cfg_path);
    } else if (access(global_cfg_path, R_OK) == 0) {
    /* if there is a global conf, parse it and then try to parse local conf  */
        MSG("INFO: found global configuration file %s, trying to parse it\n", global_cfg_path);
        parse_SX1301_configuration(global_cfg_path);
        parse_gateway_configuration(global_cfg_path);
        if (access(local_cfg_path, R_OK) == 0) {
            MSG("INFO: found local configuration file %s, trying to parse it\n", local_cfg_path);
            parse_SX1301_configuration(local_cfg_path);
            parse_gateway_configuration(local_cfg_path);
        }
    } else if (access(local_cfg_path, R_OK) == 0) {
    /* if there is only a local conf, parse it and that's all */
        MSG("INFO: found local configuration file %s, trying to parse it\n", local_cfg_path);
        parse_SX1301_configuration(local_cfg_path);
        parse_gateway_configuration(local_cfg_path);
    } else {
        MSG("ERROR: failed to find any configuration file named %s, %s or %s\n", global_cfg_path, local_cfg_path, debug_cfg_path);
        return EXIT_FAILURE;
    }
    
    /* get timezone info */
    tzset();

    /* starting the concentrator */
    i = lgw_start();
    if (i == LGW_HAL_SUCCESS) {
        MSG("INFO: [main] concentrator started, packet can now be received\n");
    } else {
        MSG("ERROR: [main] failed to start the concentrator\n");
        exit(EXIT_FAILURE);
    }

    /* spawn threads to manage upstream and downstream */
    i = pthread_create( &thrid_up, NULL, (void * (*)(void *))thread_up, NULL);
    if (i != 0) {
        MSG("ERROR: [main] impossible to create upstream thread\n");
        exit(EXIT_FAILURE);
    }

    // i = pthread_create( &thrid_down, NULL, (void * (*)(void *))thread_down, NULL);
    // if (i != 0) {
    //     MSG("ERROR: [main] impossible to create downstream thread\n");
    //     exit(EXIT_FAILURE);
    // }

    MSG("DEBUG: MAIN\n");
    
    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
    sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
    sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */

    /* transform the MAC address into a string */
    sprintf(lgwm_str, "%08X%08X", (uint32_t)(lgwm >> 32), (uint32_t)(lgwm & 0xFFFFFFFF));

    /* opening log file and writing CSV header*/
    time(&now_time);
    open_rxlog();
    

    /* main loop task : statistics collection */
    while (1) {

    }


    return EXIT_FAILURE;
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 1: RECEIVING PACKETS AND LOGGING THEM ---------------------- */

void thread_up(void) 
{
	int e = 0;
	
	struct timespec sleep_time = {0, 3000000}; /* 3 ms */
	/* local timestamp variables until we get accurate GPS time */
    struct timespec fetch_time;
    char fetch_timestamp[30];
    struct tm * x;
	
	/* allocate memory for packet fetching and processing */
    struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX]; /* array containing inbound packets + metadata */
    struct lgw_pkt_rx_s *p; /* pointer on a RX packet */
    int nb_pkt;
	
	/* data buffers */
    uint8_t buff_up[TX_BUFF_SIZE]; /* buffer to compose the upstream packet */
    int buff_index;
	
	Std_ReturnType ret = STD_NOT_OK;
    uint8_t parsedBuff[40];

	LoRaMacMessageData_t macMsgData = {0};
    LoRaMacMessageData_t ack_msg = {0};
	
	/* tx parameters */
    uint8_t status_var;

    char mod[64] = DEFAULT_MODULATION;
    uint32_t f_target = 924600000; /* target frequency - invalid default value, has to be specified by user */
    int sf = 10; /* SF7 by default */
    int cr = 4; /* CR1 aka 4/5 by default */
    int bw = 125; /* 125kHz bandwidth by default */
    int pow = 20; /* 14 dBm by default */
    int preamb = 8; /* 8 symbol preamble by default */
    int pl_size = 0; /* 16 bytes payload by default */
    int delay = 1000; /* 1 second between packets by default */
    int repeat = -1; /* by default, repeat until stopped */
    bool invert = false;
    float br_kbps = DEFAULT_BR_KBPS;
    uint8_t fdev_khz = DEFAULT_FDEV_KHZ;
    bool lbt_enable = false;
    uint32_t lbt_f_target = 0;
    uint32_t lbt_sc_time = 5000;
    int8_t lbt_rssi_target_dBm = -80;
    int8_t lbt_rssi_offset_dB = DEFAULT_SX127X_RSSI_OFFSET;
    uint8_t  lbt_nb_channel = 1;
    uint32_t sx1301_count_us;
    uint32_t tx_notch_freq = DEFAULT_NOTCH_FREQ;

    /* allocate memory for packet sending */
    struct lgw_pkt_tx_s txpkt; /* array containing 1 outbound packet + metadata */

     /* fill-up payload and parameters */
    memset(&txpkt, 0, sizeof(txpkt));
    txpkt.freq_hz = f_target;
    if (lbt_enable == true) {
        txpkt.tx_mode = TIMESTAMPED;
    } else {
        txpkt.tx_mode = IMMEDIATE;
    }
    txpkt.rf_chain = TX_RF_CHAIN;
    txpkt.rf_power = pow;
    if( strcmp( mod, "FSK" ) == 0 ) {
        txpkt.modulation = MOD_FSK;
        txpkt.datarate = br_kbps * 1e3;
        txpkt.f_dev = fdev_khz;
    } else {
        txpkt.modulation = MOD_LORA;
        switch (bw) {
            case 125: txpkt.bandwidth = BW_125KHZ; break;
            case 250: txpkt.bandwidth = BW_250KHZ; break;
            case 500: txpkt.bandwidth = BW_500KHZ; break;
            default:
                MSG("ERROR: invalid 'bw' variable\n");
                return EXIT_FAILURE;
        }
        switch (sf) {
            case  7: txpkt.datarate = DR_LORA_SF7;  break;
            case  8: txpkt.datarate = DR_LORA_SF8;  break;
            case  9: txpkt.datarate = DR_LORA_SF9;  break;
            case 10: txpkt.datarate = DR_LORA_SF10; break;
            case 11: txpkt.datarate = DR_LORA_SF11; break;
            case 12: txpkt.datarate = DR_LORA_SF12; break;
            default:
                MSG("ERROR: invalid 'sf' variable\n");
                return EXIT_FAILURE;
        }
        switch (cr) {
            case 1: txpkt.coderate = CR_LORA_4_5; break;
            case 2: txpkt.coderate = CR_LORA_4_6; break;
            case 3: txpkt.coderate = CR_LORA_4_7; break;
            case 4: txpkt.coderate = CR_LORA_4_8; break;
            default:
                MSG("ERROR: invalid 'cr' variable\n");
                return EXIT_FAILURE;
        }
    }
    txpkt.invert_pol = invert;
    txpkt.preamble = preamb;
    
    while ((quit_sig != 1) && (exit_sig != 1)) 
	{
		/* fetch packets */
        pthread_mutex_lock(&mx_concent);
        nb_pkt = lgw_receive(NB_PKT_MAX, rxpkt);
		//printf("Packet Number : %d\r\n", nb_pkt);
        pthread_mutex_unlock(&mx_concent);
        if (nb_pkt == LGW_HAL_ERROR) {
            MSG("ERROR: failed packet fetch, exiting\n");
            return EXIT_FAILURE;
        } else if (nb_pkt == 0) {
            clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_time, NULL); /* wait a short time if no packets */
        } else {
            /* local timestamp generation until we get accurate GPS time */
            clock_gettime(CLOCK_REALTIME, &fetch_time);
            x = gmtime(&(fetch_time.tv_sec));
            sprintf(fetch_timestamp,"%04i-%02i-%02i %02i:%02i:%02i.%03liZ",(x->tm_year)+1900,(x->tm_mon)+1,x->tm_mday,x->tm_hour,x->tm_min,x->tm_sec,(fetch_time.tv_nsec)/1000000); /* ISO 8601 format */
        }
		
		
		for(int packet_count = 0; packet_count < nb_pkt; ++packet_count)
		{
			p = &rxpkt[packet_count];
			
			if(p->status != STAT_CRC_OK) break;
			
			printf("msg is received\n");
			
			for(uint8_t buf_index = 0; buf_index < p->size; buf_index++)
            {
                macMsgData.Buffer[buf_index] = p->payload[buf_index];
            }
			
			macMsgData.BufSize = p->size;
			ret = LoRaMac_ParserData(&macMsgData);
	        if(ret == STD_NOT_OK)
	        {
				printf("Parse Packet Fail\r\n");
                return ret;
            }
			printf("[DEBUG] DevAddr = %u \r\n",macMsgData.FHDR.DevAddr);
			if(((macMsgData.FHDR.DevAddr)&0xFFFF0000)!=GW_ADDR)
            {
                printf("Wrong GW ADDR \r\n");
                break;
            }
			
			printf("This msg %u from node %u : %s\r\n", macMsgData.FHDR.FCnt, (macMsgData.FHDR.DevAddr) & 0xFF, macMsgData.FRMPayload);
      
      printf("Using quantize model to predict. Model is predicting...\n");
      
      time_now = clock();
      
      Py_Initialize();
    
      PyObject* sysPath = PySys_GetObject("path");
    
      PyList_Insert(sysPath, 0, PyUnicode_FromString("/home/sanslab/Desktop/tf_pi/rx_service/src/"));
    
      PyObject* module_string = PyUnicode_FromString((char*)"predict_quantize");
    
      PyObject* py_module = PyImport_Import(module_string);
    
      if (py_module == NULL) {
      printf("ERROR importing module\n");
      exit(-1);
      }
    
      float* data_split = extract_data(macMsgData.FRMPayload);
    
      PyObject* args = Py_BuildValue("iiiiiiii",(int)data_split[0],(int)data_split[1],(int)data_split[2],(int)data_split[3],(int)data_split[4],(int)data_split[5],(int)data_split[6],(int)data_split[7]);
    
      PyObject* py_function = PyObject_GetAttrString(py_module,(char*)"predict_list_params");
    
      if (py_function == NULL) {
      printf("ERROR get function\n");
      exit(-1);
      }
    
      PyObject* py_result = PyObject_CallObject(py_function, args);
    
      if (py_result == NULL) {
      printf("ERROR call function\n");
      exit(-1);
      }
    
      printf("Data predict: %s\n", PyUnicode_AsUTF8(py_result));
      
      //Calculate time predict
      time_now = clock() - time_now;
      
      time_taken = ((double)time_now)/CLOCKS_PER_SEC;
      
      printf("Time predict: %fs.\n", time_taken);
      
      // Send data predict to AWS broker
      
      printf("Send data to AWS broker...\n");
      
      module_string = PyUnicode_FromString((char*)"publish_broker");
    
      py_module = PyImport_Import(module_string);
    
      if (py_module == NULL) {
          printf("ERROR importing module publish\n");
          exit(-1);
      }
    
      args = Py_BuildValue("ss", "28.5,32.7,8.2,0.017,0.003,6.491,6.3,4.7", PyUnicode_AsUTF8(py_result));
    
      py_function = PyObject_GetAttrString(py_module,(char*)"publishData");
    
      if (py_function == NULL) {
          printf("ERROR get function publish\n");
          exit(-1);
      }
    
      py_result = PyObject_CallObject(py_function, args);
      
      printf("Send data success\n");
    
      Py_FinalizeEx();
    
			/* Log Data */
            /* writing Node Address */
            fprintf(rxfile,"%u.%u.%u.%u,",(macMsgData.FHDR.DevAddr >> 24) & 0xFF,(macMsgData.FHDR.DevAddr >> 16) & 0xFF,(macMsgData.FHDR.DevAddr >> 8) & 0xFF,(macMsgData.FHDR.DevAddr) & 0xFF);
            /* writing RX frequency */
            fprintf(rxfile, "%10u,", p->freq_hz);
            /* writing payload size */
            fprintf(rxfile,"%3u,", p->size);
			
			/* writing bandwidth */
            switch(p->bandwidth) {
                case BW_500KHZ:     fputs("500000,", rxfile); break;
                case BW_250KHZ:     fputs("250000,", rxfile); break;
                case BW_125KHZ:     fputs("125000,", rxfile); break;
                case BW_62K5HZ:     fputs("62500 ,", rxfile); break;
                case BW_31K2HZ:     fputs("31200 ,", rxfile); break;
                case BW_15K6HZ:     fputs("15600 ,", rxfile); break;
                case BW_7K8HZ:      fputs("7800  ,", rxfile); break;
                case BW_UNDEFINED:  fputs("0     ,", rxfile); break;
                default:            fputs("-1    ,", rxfile);
            }
			
			
			/* writing datarate */
            if (p->modulation == MOD_LORA) {
                switch (p->datarate) {
                    case DR_LORA_SF7:   fputs("\"SF7\"  ,", rxfile); break;
                    case DR_LORA_SF8:   fputs("\"SF8\"  ,", rxfile); break;
                    case DR_LORA_SF9:   fputs("\"SF9\"  ,", rxfile); break;
                    case DR_LORA_SF10:  fputs("\"SF10\" ,", rxfile); break;
                    case DR_LORA_SF11:  fputs("\"SF11\" ,", rxfile); break;
                    case DR_LORA_SF12:  fputs("\"SF12\" ,", rxfile); break;
                    default:            fputs("\"ERR\"  ,", rxfile);
                }
            } else if (p->modulation == MOD_FSK) {
                fprintf(rxfile, "\"%6u\",", p->datarate);
            } else {
                fputs("\"ERR\"   ,", rxfile);
            }
			
			/* writing coderate */
            switch (p->coderate) {
                case CR_LORA_4_5:   fputs("\"4/5\",", rxfile); break;
                case CR_LORA_4_6:   fputs("\"2/3\",", rxfile); break;
                case CR_LORA_4_7:   fputs("\"4/7\",", rxfile); break;
                case CR_LORA_4_8:   fputs("\"1/2\",", rxfile); break;
                case CR_UNDEFINED:  fputs("\"\"   ,", rxfile); break;
                default:            fputs("\"ERR\",", rxfile);
            }
            /* writing packet RSSI */
            fprintf(rxfile, "%+.0f,", p->rssi);
            /* writing packet average SNR */
            fprintf(rxfile, "%+5.1f,", p->snr);
            /* writing Frame Counter */
            fprintf(rxfile,"%u,",macMsgData.FHDR.FCnt);
            /* writing Frame Payload */
            fprintf(rxfile,"%s",macMsgData.FRMPayload);
            fputs("\n", rxfile); //end of log file line
            fflush(rxfile);
            // delay to send
            wait_ms(10);
            /* Setup ACK to send */
            printf("%u\r\n",macMsgData.FHDR.DevAddr >>24);
            LoRaMac_SetUpACKMessage(&ack_msg,macMsgData.FHDR.DevAddr,macMsgData.FHDR.FCnt);
			
			for(int pkt_index = 0; pkt_index < ack_msg.BufSize; pkt_index++)
            {
                txpkt.payload[pkt_index] = ack_msg.Buffer[pkt_index];
            }
			
			printf("%u\r\n",ack_msg.BufSize);
            txpkt.size = ack_msg.BufSize;
            /* send packet */
	        wait_ms(50);
            printf("Sending ACK\r\n");
			
			pthread_mutex_lock(&mx_concent);
            e = lgw_send(txpkt); 
            pthread_mutex_unlock(&mx_concent);
			
			
			if (e == LGW_HAL_ERROR) 
            {
                printf("ERROR\n");
                return EXIT_FAILURE;
            } 
            else if (e == LGW_LBT_ISSUE ) 
            {
                printf("Failed: Not allowed (LBT)\n");
            } 
            else 
            {
                /* wait for packet to finish sending */
                do 
                {
                    wait_ms(5);
                    pthread_mutex_lock(&mx_concent);
                    lgw_status(TX_STATUS, &status_var); /* get TX status */
                    pthread_mutex_unlock(&mx_concent);
                } while (status_var != TX_FREE);
				printf("Send ACK OK\n");
            }
			
			printf("\r\n");
            /* wait inter-packet delay */
            wait_ms(delay);
        }
		
		/* exit loop on user signals */
        if ((quit_sig == 1) || (exit_sig == 1)) 
		{
            break;
        }
	}
    return EXIT_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 2: READ DOWNSTREAM LOG FILE AND SENDING THEM ---------- */

void thread_down(void) 
{
    int i;

    return EXIT_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
