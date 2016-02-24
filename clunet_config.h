/* Name: clunet_config.h
 * Project: CLUNET network driver
 * Author: Alexey Avdyukhin
 * Creation Date: 2012-11-08
 * License: DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * Atmega328 by gunmetal
 */
 
#ifndef __clunet_config_h_included__
#define __clunet_config_h_included__

#define MAX_NAME_LENGTH 64

/* Device name */
#define CLUNET_DEVICE_NAME "CLUNET device 1"

/* Buffer sized (memory usage) */
#define CLUNET_SEND_BUFFER_SIZE 255
#define CLUNET_READ_BUFFER_SIZE 255

/* Pin to send data */
#define CLUNET_WRITE_PORT D
#define CLUNET_WRITE_PIN 4

/* Using transistor? */
#define CLUNET_WRITE_TRANSISTOR

/* Pin to receive data, external interrupt required! */
#define CLUNET_READ_PORT D
#define CLUNET_READ_PIN 2

/* Timer prescaler */
#define CLUNET_TIMER_PRESCALER 64

/* Custom T (T >= 8 && T <= 24). 
 T is frame unit size in timer ticks. Lower - faster, highter - more stable
 If not defined T will be calculated as ~64us based on CLUNET_TIMER_PRESCALER value
*/
 //#define CLUNET_T 8

/* Timer initialization */
#define CLUNET_TIMER2_INIT {unset_bit4(TCCR2A, WGM21, WGM20, COM2A1, COM2A0); /* Timer2, normal mode */ \
	set_bit(TCCR2B, CS22); unset_bit2(TCCR2B, CS21, CS20); /* 64x prescaler */ }
	
#define CLUNET_BROADCAST_ADDRESS 0xFF

/* Timer registers */
#define CLUNET_TIMER_REG TCNT2
#define CLUNET_TIMER_REG_OCR OCR2A

/* How to enable and disable timer interrupts */
#define CLUNET_ENABLE_TIMER_COMP set_bit(TIMSK2, OCIE2A)
#define CLUNET_DISABLE_TIMER_COMP unset_bit(TIMSK2, OCIE2A)
#define CLUNET_ENABLE_TIMER_OVF set_bit(TIMSK2, TOIE1)
#define CLUNET_DISABLE_TIMER_OVF unset_bit(TIMSK2, TOIE1)

/* Interrupt vectors */
#define CLUNET_TIMER_COMP_VECTOR TIMER2_COMPA_vect
#define CLUNET_TIMER_OVF_VECTOR TIMER2_OVF_vect
#define CLUNET_INT_VECTOR INT0_vect

#ifndef CLUNET_T
#define CLUNET_T ((F_CPU / CLUNET_TIMER_PRESCALER) / (250000 / 16))
#endif
#if CLUNET_T < 8
#  error Timer frequency is too small, increase CPU frequency or decrease timer prescaler
#endif
#if CLUNET_T > 24
#  error Timer frequency is too big, decrease CPU frequency or increase timer prescaler
#endif

#define CLUNET_0_T (CLUNET_T)
#define CLUNET_1_T (3*CLUNET_T)
#define CLUNET_INIT_T (10*CLUNET_T)

#define CLUNET_CONCAT(a, b)            a ## b
#define CLUNET_OUTPORT(name)           CLUNET_CONCAT(PORT, name)
#define CLUNET_INPORT(name)            CLUNET_CONCAT(PIN, name)
#define CLUNET_DDRPORT(name)           CLUNET_CONCAT(DDR, name)

#ifdef CLUNET_WRITE_TRANSISTOR
#  define CLUNET_SEND_1 CLUNET_OUTPORT(CLUNET_WRITE_PORT) |= (1 << CLUNET_WRITE_PIN)
#  define CLUNET_SEND_0 CLUNET_OUTPORT(CLUNET_WRITE_PORT) &= ~(1 << CLUNET_WRITE_PIN)
#  define CLUNET_SENDING (CLUNET_OUTPORT(CLUNET_WRITE_PORT) & (1 << CLUNET_WRITE_PIN))
#  define CLUNET_SEND_INVERT CLUNET_OUTPORT(CLUNET_WRITE_PORT) ^= (1 << CLUNET_WRITE_PIN)
#  define CLUNET_SEND_INIT { CLUNET_DDRPORT(CLUNET_WRITE_PORT) |= (1 << CLUNET_WRITE_PIN); CLUNET_SEND_0; }
#else
#  define CLUNET_SEND_1 CLUNET_DDRPORT(CLUNET_WRITE_PORT) |= (1 << CLUNET_WRITE_PIN)
#  define CLUNET_SEND_0 CLUNET_DDRPORT(CLUNET_WRITE_PORT) &= ~(1 << CLUNET_WRITE_PIN)
#  define CLUNET_SENDING (CLUNET_DDRPORT(CLUNET_WRITE_PORT) & (1 << CLUNET_WRITE_PIN))
#  define CLUNET_SEND_INVERT CLUNET_DDRPORT(CLUNET_WRITE_PORT) ^= (1 << CLUNET_WRITE_PIN)
#  define CLUNET_SEND_INIT { CLUNET_OUTPORT(CLUNET_WRITE_PORT) &= ~(1 << CLUNET_WRITE_PIN); CLUNET_SEND_0; }
#endif

#define CLUNET_READ_INIT { CLUNET_DDRPORT(CLUNET_READ_PORT) &= ~(1 << CLUNET_READ_PIN); CLUNET_OUTPORT(CLUNET_READ_PORT) |= (1 << CLUNET_READ_PIN); }
#define CLUNET_READING (!(CLUNET_INPORT(CLUNET_READ_PORT) & (1 << CLUNET_READ_PIN)))

#ifndef CLUNET_SEND_BUFFER_SIZE
#  error CLUNET_SEND_BUFFER_SIZE is not defined
#endif
#ifndef CLUNET_READ_BUFFER_SIZE
#  error CLUNET_READ_BUFFER_SIZE is not defined
#endif
#if CLUNET_SEND_BUFFER_SIZE > 255
#  error CLUNET_SEND_BUFFER_SIZE must be <= 255
#endif
#if CLUNET_READ_BUFFER_SIZE > 255
#  error CLUNET_READ_BUFFER_SIZE must be <= 255
#endif

#endif
