#include "clunet.h"


void data_received(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size)
{
    Serial.print("src_address: ");
    Serial.print(src_address);
    Serial.print(", dst_address: ");
    Serial.print(dst_address);
    Serial.print(", cmd: ");
    Serial.print(command);
    Serial.print(", data: ");
    Serial.println(data);
}

Clunet clunet;

void setup (void)
{
    Serial.begin(115200);
    clunet.set_on_data_received(data_received);
    clunet.init();
}


void loop (void)
{
    char buffer[1];
    buffer[0] = 1;
    clunet.send(CLUNET_BROADCAST_ADDRESS, CLUNET_PRIORITY_MESSAGE, CLUNET_COMMAND_DEVICE_POWER_INFO, buffer, sizeof(buffer));

    delay(1000);
}
