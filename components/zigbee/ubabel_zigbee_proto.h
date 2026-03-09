#ifndef UBABEL_ZB_PROTO_H
#define UBABEL_ZB_PROTO_H

#define UBABEL_CUSTOM_CLUSTER_ID        0xFF00

#define UBABEL_ATTR_SENSOR_READING_ID   0x0001  // uint16, end device -> coordinator
#define UBABEL_ATTR_COMMAND_ID          0x0002  // uint8,  coordinator -> end device
#define UBABEL_ATTR_SYNC_DATA_ID        0x0003  // octet string, bidirectional

#endif // !UBABEL_ZB_PROTO_H
