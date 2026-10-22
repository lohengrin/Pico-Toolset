// BTstack configuration for pico_toolset_bluetooth: BLE central + peripheral
// roles only (advertise + scan), no Classic. Adapted from pico-examples'
// pico_w/bt/config/btstack_config.h (BSD-3-Clause, Raspberry Pi Ltd).
#ifndef PICO_TOOLSET_BTSTACK_CONFIG_H
#define PICO_TOOLSET_BTSTACK_CONFIG_H

#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP // required by the SDK's stdout HCI dump
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_SOFTWARE_AES128
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_GATT_CLIENT_PAIRING

#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define HCI_ACL_PAYLOAD_SIZE (255 + 4)
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4

// Room for a keyboard, a gamepad and a mouse at once (bluetooth_hid).
#define MAX_NR_GATT_CLIENTS 3
#define MAX_NR_HIDS_HOSTS 3 // BTstack 1.6.x name (hids_client was renamed hids_host); 0 = "Not enough memory to create client"
#define MAX_NR_HCI_CONNECTIONS 3
#define MAX_NR_L2CAP_CHANNELS 4
#define MAX_NR_L2CAP_SERVICES 2
#define MAX_NR_SM_LOOKUP_ENTRIES 6
#define MAX_NR_WHITELIST_ENTRIES 8
#define MAX_NR_LE_DEVICE_DB_ENTRIES 8
#define MAX_NR_CONTROLLER_ACL_BUFFERS 3
#define MAX_NR_CONTROLLER_SCO_PACKETS 3
#define MAX_ATT_DB_SIZE 512
#define NVM_NUM_DEVICE_DB_ENTRIES 8
#define NVM_NUM_LINK_KEYS 4

#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#define HCI_HOST_ACL_PACKET_LEN 255
#define HCI_HOST_ACL_PACKET_NUM 3
#define HCI_HOST_SCO_PACKET_LEN 120
#define HCI_HOST_SCO_PACKET_NUM 3

#ifdef ENABLE_CLASSIC
// Bluetooth Classic HID host (bluetooth_hid): each connection uses an L2CAP control and
// interrupt channel, plus a temporary SDP client channel to read the HID descriptor.
#define MAX_NR_HID_HOST_CONNECTIONS 2
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 4
#define MAX_ATTRIBUTE_VALUE_SIZE 512
#undef MAX_NR_L2CAP_CHANNELS
#define MAX_NR_L2CAP_CHANNELS 8
#undef MAX_NR_L2CAP_SERVICES
#define MAX_NR_L2CAP_SERVICES 4
#undef NVM_NUM_LINK_KEYS
#define NVM_NUM_LINK_KEYS 8
#endif

#ifdef PICO_TOOLSET_BLUETOOTH_A2DP
// bluetooth_a2dp (A2DP source + AVRCP controller): one AVDTP connection with one stream endpoint,
// one AVRCP connection, three SDP records. Only defined when that component is linked.
#define MAX_NR_AVDTP_CONNECTIONS 1
#define MAX_NR_AVDTP_STREAM_ENDPOINTS 1
#define MAX_NR_AVRCP_CONNECTIONS 2
#define MAX_NR_SERVICE_RECORD_ITEMS 3
// Media packets are bounded by the L2CAP MTU, which follows the HCI ACL buffer: at the default 255
// bytes an SBC packet holds only 3 frames and the stream needs ~115 packets/s.
#undef HCI_ACL_PAYLOAD_SIZE
#define HCI_ACL_PAYLOAD_SIZE (1021 + 4)
#undef MAX_NR_HCI_CONNECTIONS
#define MAX_NR_HCI_CONNECTIONS 5
#undef MAX_NR_L2CAP_CHANNELS
#define MAX_NR_L2CAP_CHANNELS 10
#undef MAX_NR_L2CAP_SERVICES
#define MAX_NR_L2CAP_SERVICES 6
#endif

#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT
#define HCI_RESET_RESEND_TIMEOUT_MS 1000

#endif
