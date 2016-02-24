/* Name: clunet.c
 * Project: CLUNET network driver
 * Author: Alexey Avdyukhin
 * Creation Date: 2013-09-09
 * License: DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 */

#include "bits.h"
#include "clunet.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include "Arduino.h"


#define CLUNET_SENDING_STATE_IDLE 0
#define CLUNET_SENDING_STATE_INIT 1
#define CLUNET_SENDING_STATE_PRIO1 2
#define CLUNET_SENDING_STATE_PRIO2 3
#define CLUNET_SENDING_STATE_DATA 4
#define CLUNET_SENDING_STATE_WAITING_LINE 6
#define CLUNET_SENDING_STATE_PREINIT 7
#define CLUNET_SENDING_STATE_STOP 8
#define CLUNET_SENDING_STATE_DONE 9

#define CLUNET_READING_STATE_IDLE 0
#define CLUNET_READING_STATE_INIT 1
#define CLUNET_READING_STATE_PRIO1 2
#define CLUNET_READING_STATE_PRIO2 3
#define CLUNET_READING_STATE_HEADER 4
#define CLUNET_READING_STATE_DATA 5

#define CLUNET_OFFSET_SRC_ADDRESS 0
#define CLUNET_OFFSET_DST_ADDRESS 1
#define CLUNET_OFFSET_COMMAND 2
#define CLUNET_OFFSET_SIZE 3
#define CLUNET_OFFSET_DATA 4


Clunet *Clunet::instance = 0;

Clunet::Clunet(unsigned char devId)
{
    clunetSendingState = CLUNET_SENDING_STATE_IDLE;
    clunetReadingState = CLUNET_READING_STATE_IDLE;

    clunetReceivingState = 0;
    // clunetReceivingPrio = 0;
    clunetTimerStart = 0;
    clunetTimerPeriods = 0;

    on_data_received = 0;
    on_data_received_sniff = 0;

#ifndef CLUNET_DEVICE_NAME
    strncpy(devName, CLUNET_DEVICE_NAME, MAX_NAME_LENGTH);
#else
    devName[0] = '\n';
#endif

    device_id = devId;

    Clunet::instance = this;
}

void Clunet::handleTimerComp()
{
    unsigned char now = CLUNET_TIMER_REG;     // Запоминаем текущее время

    switch (clunetSendingState)
    {
            case CLUNET_SENDING_STATE_PREINIT: // Нужно подождать перед отправкой
                    CLUNET_TIMER_REG_OCR = now + CLUNET_T;
                    clunetSendingState = CLUNET_SENDING_STATE_INIT;  // Начинаем следующую фазу
                    return;
            case CLUNET_SENDING_STATE_STOP:	// Завершение передачи, но надо ещё подождать
                    CLUNET_SEND_0;				// Отпускаем линию
                    CLUNET_TIMER_REG_OCR = now + CLUNET_T;
                    clunetSendingState = CLUNET_SENDING_STATE_DONE;
                    return;
            case CLUNET_SENDING_STATE_DONE:	// Завершение передачи
                    CLUNET_DISABLE_TIMER_COMP; // Выключаем таймер-сравнение
                    clunetSendingState = CLUNET_SENDING_STATE_IDLE; // Ставим флаг, что передатчик свободен
                    return;
    }

    if (/*!((clunetReadingState == CLUNET_READING_STATE_DATA) && // Если мы сейчас не [получаем данные
            (clunetCurrentPrio > clunetReceivingPrio) 				// И приоритет получаемых данных не ниже
            && (clunetSendingState == CLUNET_SENDING_STATE_INIT))  // И мы не пытаемся начать инициализацию]
            &&*/ (!CLUNET_SENDING && CLUNET_READING)) // То идёт проверка на конфликт. Если мы линию не держим, но она уже занята
    {
            CLUNET_DISABLE_TIMER_COMP; // Выключаем таймер-сравнение
            clunetSendingState = CLUNET_SENDING_STATE_WAITING_LINE; // Переходим в режим ожидания линии
            return;											 // И умолкаем
    }

    CLUNET_SEND_INVERT;	 // Сразу инвртируем значение сигнала, у нас это запланировано

    if (!CLUNET_SENDING)        // Если мы отпустили линию...
    {
            CLUNET_TIMER_REG_OCR = now + CLUNET_T; // То вернёмся в это прерывание через CLUNET_T единиц времени
            return;
    }
    switch (clunetSendingState)
    {
            case CLUNET_SENDING_STATE_INIT: // Инициализация
                    CLUNET_TIMER_REG_OCR = now + CLUNET_INIT_T;
                    clunetSendingState = CLUNET_SENDING_STATE_PRIO1;  // Начинаем следующую фазу
                    return;
            case CLUNET_SENDING_STATE_PRIO1: // Фаза передачи приоритета, старший бит
                    if ((clunetCurrentPrio-1) & 2) // Если 1, то ждём 3T, а если 0, то 1T
                            CLUNET_TIMER_REG_OCR = now + CLUNET_1_T;
                    else CLUNET_TIMER_REG_OCR = now + CLUNET_0_T;
                    clunetSendingState = CLUNET_SENDING_STATE_PRIO2;
                    return;
            case CLUNET_SENDING_STATE_PRIO2: // Фаза передачи приоритета, младший бит
                    if ((clunetCurrentPrio-1) & 1) // Если 1, то ждём 3T, а если 0, то 1T
                            CLUNET_TIMER_REG_OCR = now + CLUNET_1_T;
                    else CLUNET_TIMER_REG_OCR = now + CLUNET_0_T;
                    clunetSendingState = CLUNET_SENDING_STATE_DATA;
                    return;
            case CLUNET_SENDING_STATE_DATA: // Фаза передачи данных
                    if (dataToSend[clunetSendingCurrentByte] & (1 << clunetSendingCurrentBit)) // Если 1, то ждём 3T, а если 0, то 1T
                            CLUNET_TIMER_REG_OCR = now + CLUNET_1_T;
                    else CLUNET_TIMER_REG_OCR = now + CLUNET_0_T;
                    clunetSendingCurrentBit++; // Переходим к следующему биту
                    if (clunetSendingCurrentBit >= 8)
                    {
                            clunetSendingCurrentBit = 0;
                            clunetSendingCurrentByte++;
                    }
                    if (clunetSendingCurrentByte >= clunetSendingDataLength) // Данные закончились
                    {
                            clunetSendingState = CLUNET_SENDING_STATE_STOP; // Следующая фаза
                    }
                    return;
    }
}

void Clunet::handleTimerOvf()
{
    if (clunetTimerPeriods < 3)
    {
        clunetTimerPeriods++;
    }
    else // Слишком долго нет сигнала, сброс и отключение прерывания
    {
            CLUNET_SEND_0; 					// А вдруг мы забыли линию отжать? Хотя по идее не должно...
            clunetReadingState = CLUNET_READING_STATE_IDLE;
            if ((clunetSendingState == CLUNET_SENDING_STATE_IDLE) && (!CLUNET_READING))
                    CLUNET_DISABLE_TIMER_OVF;
            if ((clunetSendingState == CLUNET_SENDING_STATE_WAITING_LINE) && (!CLUNET_READING)) // Если есть неотосланные данные, шлём, линия освободилась
                    start_send();
    }
}

void Clunet::handleInterrupt()
{
    unsigned char time = (unsigned char)((CLUNET_TIMER_REG-clunetTimerStart) & 0xFF);
    if (!CLUNET_READING) // Линию отпустило
    {
            CLUNET_ENABLE_TIMER_OVF;
            if (time >= (CLUNET_INIT_T+CLUNET_1_T)/2) // Если кто-то долго жмёт линию, это инициализация
            {
                    clunetReadingState = CLUNET_READING_STATE_PRIO1;
            }
            else switch (clunetReadingState) // А если не долго, то смотрим на этап
            {
                    case CLUNET_READING_STATE_PRIO1:    // Получение приоритета, клиенту он не нужен
                    /*
                            if (time > (CLUNET_0_T+CLUNET_1_T)/2)
                                    clunetReceivingPrio = 3;
                                    else clunetReceivingPrio = 1;
                    */
                            clunetReadingState = CLUNET_READING_STATE_PRIO2;
                            break;
                    case CLUNET_READING_STATE_PRIO2:	 // Получение приоритета, клиенту он не нужен
                    /*
                            if (time > (CLUNET_0_T+CLUNET_1_T)/2)
                                    clunetReceivingPrio++;
                    */
                            clunetReadingState = CLUNET_READING_STATE_DATA;
                            clunetReadingCurrentByte = 0;
                            clunetReadingCurrentBit = 0;
                            dataToRead[0] = 0;
                            break;
                    case CLUNET_READING_STATE_DATA:    // Чтение всех данных
                            if (time > (CLUNET_0_T+CLUNET_1_T)/2)
                                    dataToRead[clunetReadingCurrentByte] |= (1 << clunetReadingCurrentBit);
                            clunetReadingCurrentBit++;
                            if (clunetReadingCurrentBit >= 8)  // Переходим к следующему байту
                            {
                                    clunetReadingCurrentByte++;
                                    clunetReadingCurrentBit = 0;
                                    if (clunetReadingCurrentByte < CLUNET_READ_BUFFER_SIZE)
                                            dataToRead[clunetReadingCurrentByte] = 0;
                                    else // Если буфер закончился
                                    {
                                            clunetReadingState = CLUNET_READING_STATE_IDLE;
                                            return;
                                    }
                            }
                            if ((clunetReadingCurrentByte > CLUNET_OFFSET_SIZE) && (clunetReadingCurrentByte > dataToRead[CLUNET_OFFSET_SIZE]+CLUNET_OFFSET_DATA))
                            {
                                    // Получили данные полностью, ура!
                                    clunetReadingState = CLUNET_READING_STATE_IDLE;
                                    char crc = check_crc((char*)dataToRead,clunetReadingCurrentByte); // Проверяем CRC
                                    if (crc == 0)
                                            data_received(dataToRead[CLUNET_OFFSET_SRC_ADDRESS], dataToRead[CLUNET_OFFSET_DST_ADDRESS], dataToRead[CLUNET_OFFSET_COMMAND], (char*)(dataToRead+CLUNET_OFFSET_DATA), dataToRead[CLUNET_OFFSET_SIZE]);
                            }
                            break;
                    default:
                            break;
            }
    }
    else
    {
            clunetTimerStart = CLUNET_TIMER_REG;
            clunetTimerPeriods = 0;
            CLUNET_ENABLE_TIMER_OVF;
    }
}

char Clunet::check_crc(char* data, unsigned char size)
{
    uint8_t crc=0;
    uint8_t i,j;
    for (i=0; i<size;i++)
    {
        uint8_t inbyte = data[i];
        for (j=0;j<8;j++)
        {
              uint8_t mix = (crc ^ inbyte) & 0x01;
              crc >>= 1;
              if (mix)
                    crc ^= 0x8C;

              inbyte >>= 1;
        }
    }
    return crc;
}

ISR(CLUNET_TIMER_COMP_VECTOR)
{		
    Clunet::instance->handleTimerComp();
}


void Clunet::start_send()
{
    CLUNET_SEND_0;
    if (clunetSendingState != CLUNET_SENDING_STATE_PREINIT) // Если не нужна пауза...
            clunetSendingState = CLUNET_SENDING_STATE_INIT; // Инициализация передачи
    clunetSendingCurrentByte = clunetSendingCurrentBit = 0; // Обнуляем счётчик
    CLUNET_TIMER_REG_OCR = CLUNET_TIMER_REG+CLUNET_T;			// Планируем таймер, обычно почему-то прерывание срабатывает сразу
    CLUNET_ENABLE_TIMER_COMP;			// Включаем прерывание таймера-сравнения
}

void Clunet::send(unsigned char address, unsigned char prio, unsigned char command, char* data, unsigned char size)
{
    if (CLUNET_OFFSET_DATA+size+1 > CLUNET_SEND_BUFFER_SIZE)
        return; // Не хватает буфера

    CLUNET_DISABLE_TIMER_COMP;CLUNET_SEND_0; // Прерываем текущую передачу, если есть такая

    // Заполняем переменные
    if (clunetSendingState != CLUNET_SENDING_STATE_PREINIT)
        clunetSendingState = CLUNET_SENDING_STATE_IDLE;

    clunetCurrentPrio = prio;
    dataToSend[CLUNET_OFFSET_SRC_ADDRESS] = device_id;
    dataToSend[CLUNET_OFFSET_DST_ADDRESS] = address;
    dataToSend[CLUNET_OFFSET_COMMAND] = command;
    dataToSend[CLUNET_OFFSET_SIZE] = size;

    unsigned char i;
    for (i = 0; i < size; i++)
        dataToSend[CLUNET_OFFSET_DATA+i] = data[i];

    dataToSend[CLUNET_OFFSET_DATA+size] = check_crc((char*)dataToSend, CLUNET_OFFSET_DATA+size);
    clunetSendingDataLength = CLUNET_OFFSET_DATA + size + 1;
    if (
            (clunetReadingState == CLUNET_READING_STATE_IDLE) // Если мы ничего не получаем в данный момент, то посылаем сразу
            //		|| ((clunetReadingState == CLUNET_READING_STATE_DATA) && (prio > clunetReceivingPrio)) // Либо если получаем, но с более низким приоритетом
            )
        start_send(); // Запускаем передачу сразу
    else
        clunetSendingState = CLUNET_SENDING_STATE_WAITING_LINE; // Иначе ждём линию
}

inline void Clunet::data_received(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size)
{	
    if (on_data_received_sniff)
            (*on_data_received_sniff)(src_address, dst_address, command, data, size);

    if (src_address == device_id) return; // Игнорируем сообщения от самого себя!

    if ((dst_address != device_id) &&
            (dst_address != CLUNET_BROADCAST_ADDRESS)) return; // Игнорируем сообщения не для нас

    if (command == CLUNET_COMMAND_REBOOT) // Просто ребут. И да, ребутнуть себя мы можем
    {
            #if defined(__AVR_ATmega8__)
              cli();
              set_bit(WDTCR, WDE);
              while(1);
            #elif defined(__AVR_ATmega328P__)
//		  cli();
//		  wdt_enable(WDTO_15MS);
//		  while(1);
            #else
            #error unknown chip
            #endif
    }

    if ((clunetSendingState == CLUNET_SENDING_STATE_IDLE) || (clunetCurrentPrio <= CLUNET_PRIORITY_MESSAGE))
    {
            if (command == CLUNET_COMMAND_DISCOVERY) // Ответ на поиск устройств
            {
                int len = 0;
                if (devName)
                    len = strlen(devName);
                clunetSendingState = CLUNET_SENDING_STATE_PREINIT;
                send(src_address, CLUNET_PRIORITY_MESSAGE, CLUNET_COMMAND_DISCOVERY_RESPONSE, devName, len);
            }
            else if (command == CLUNET_COMMAND_PING) // Ответ на пинг
            {
                    clunetSendingState = CLUNET_SENDING_STATE_PREINIT;
                    send(src_address, CLUNET_PRIORITY_COMMAND, CLUNET_COMMAND_PING_REPLY, data, size);
            }
    }

    if (on_data_received)
            (*on_data_received)(src_address, dst_address, command, data, size);

    if ((clunetSendingState == CLUNET_SENDING_STATE_WAITING_LINE) && !CLUNET_READING) // Если есть неотосланные данные, шлём, линия освободилась
    {
            clunetSendingState = CLUNET_SENDING_STATE_PREINIT;
            start_send();
    }
}

ISR(CLUNET_TIMER_OVF_VECTOR)
{		
    Clunet::instance->handleTimerOvf();
}


void ISR_CLUNET_INT_VECTOR()
{
    Clunet::instance->handleInterrupt();
}

void Clunet::init()
{
    attachInterrupt(0, ISR_CLUNET_INT_VECTOR, CHANGE);
    sei();
    CLUNET_SEND_INIT;
    CLUNET_READ_INIT;
    CLUNET_TIMER2_INIT;
#ifdef CLUNET_ENABLE_INT
    CLUNET_ENABLE_INT;
#warning CLUNET_ENABLE_INT is deprecated
#endif	
    #if defined(__AVR_ATmega8__)
    char reset_source = MCUCSR;
    #elif defined(__AVR_ATmega328P__)
    char reset_source = MCUSR;
    #else
    #error unknown chip
    #endif
    send(CLUNET_BROADCAST_ADDRESS, CLUNET_PRIORITY_MESSAGE,	CLUNET_COMMAND_BOOT_COMPLETED, &reset_source, 1);

    #if defined(__AVR_ATmega8__)
    MCUCSR = 0;
    #elif defined(__AVR_ATmega328P__)
    MCUSR = 0;
    #endif
}

int Clunet::readyToSend() // Возвращает 0, если готов к передаче, иначе приоритет текущей задачи
{
    if (clunetSendingState == CLUNET_SENDING_STATE_IDLE)
        return 0;
    return clunetCurrentPrio;
}

void Clunet::setOnDataReceived(void (*f)(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size))
{	
    on_data_received = f;
}

void Clunet::setOnDataReceivedSniff(void (*f)(unsigned char src_address, unsigned char dst_address, unsigned char command, char* data, unsigned char size))
{	
    on_data_received_sniff = f;
}

void Clunet::setDeviceName(char *name)
{
    strncpy(devName, name, MAX_NAME_LENGTH);
}

void Clunet::setDeviceId(unsigned char devId)
{
    device_id = devId;
}
