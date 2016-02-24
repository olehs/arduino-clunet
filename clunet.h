/* Name: clunet.h
 * Project: CLUNET network driver
 * Author: Alexey Avdyukhin
 * Creation Date: 2013-09-09
 * License: DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 */
 
#ifndef __clunet_h_included__
#define __clunet_h_included__

#include "bits.h"
#include "commands.h"

#include "clunet_config.h"

class Clunet
{
public:
    Clunet(unsigned char devId);

    // Инициализация
    void init();

    // Отправка пакета
    void send(unsigned char address, unsigned char prio, unsigned char command, char* data, unsigned char size);

    // Возвращает 0, если готов к передаче, иначе приоритет текущей задачи
    int readyToSend();

    // Установка функций, которые вызываются при получении пакетов
    // Эта - получает пакеты, которые адресованы нам
    void setOnDataReceived(void (*f)(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size));

    // А эта - абсолютно все, которые ходят по сети, включая наши
    void setOnDataReceivedSniff(void (*f)(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size));

    void setDeviceName(char *name);
    char* deviceName() {return devName;}

    void setDeviceId(unsigned char devId);
    unsigned char deviceId() {return device_id;}

    static Clunet *instance;
    void inline handleTimerComp();
    void inline handleTimerOvf();
    void inline handleInterrupt();

private:

    void (*on_data_received)(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size);
    void (*on_data_received_sniff)(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size);

    char check_crc(char* data, unsigned char size);

    unsigned char clunetSendingState;
    unsigned short int clunetSendingDataLength;
    unsigned char clunetSendingCurrentByte;
    unsigned char clunetSendingCurrentBit;
    unsigned char clunetReadingState;
    unsigned char clunetReadingCurrentByte;
    unsigned char clunetReadingCurrentBit;
    unsigned char clunetCurrentPrio;

    unsigned char clunetReceivingState;
//    unsigned char clunetReceivingPrio;
    unsigned char clunetTimerStart;
    unsigned char clunetTimerPeriods;

    char dataToSend[CLUNET_SEND_BUFFER_SIZE];
    char dataToRead[CLUNET_READ_BUFFER_SIZE];
    void data_received(unsigned char src_address, unsigned char dst_address, unsigned char command, char *data, unsigned char size);
    void start_send();

    char devName[MAX_NAME_LENGTH + 1];
    unsigned char device_id;
};

#endif
