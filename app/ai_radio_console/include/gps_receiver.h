#ifndef __GPS_RECEIVER_H
#define __GPS_RECEIVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GPS hardware interface documentation
 *
 * Recommended modules:
 *   - ATGM336H (BDS/GPS dual-mode, 3.3 V)
 *   - u-blox NEO-6M / NEO-7M (3.3 V)
 *
 * Wiring (Allwinner R528 / Gemini-S1):
 *   GPS TX  -> R528 UART RX
 *   GPS RX  -> R528 UART TX
 *   GPS PPS -> optional GPIO (not used by this driver)
 *   GPS VCC -> 3.3 V rail
 *   GPS GND -> system GND
 *
 * Power supply: 3.3 V only. Do not connect a 5 V module directly to the
 * R528 UART/GPIO pins.
 */

typedef struct {
    bool valid;          /*!< true when the parsed fix is valid */
    double latitude;     /*!< decimal degrees, negative = south */
    double longitude;    /*!< decimal degrees, negative = west */
    float altitude_m;    /*!< meters above mean sea level */
    float speed_kph;     /*!< ground speed in km/h */
    char utc_time[32];   /*!< UTC time string from the last sentence */
    int num_sats;        /*!< number of satellites in use (GGA) */
    int fix_quality;     /*!< fix quality: 0 = invalid, 1 = GPS fix, ... */
} gps_fix_t;

/*! Callback invoked for every parsed GGA/RMC sentence. */
typedef void (*gps_fix_cb_t)(const gps_fix_t *fix, void *user_data);

/*! Store device path and baud rate. Must be called before start. */
int gps_receiver_init(const char *dev, int baud);

/*! Start the background GPS reader thread. */
int gps_receiver_start(void);

/*! Stop the background GPS reader thread. */
int gps_receiver_stop(void);

/*! Register the callback that receives parsed fixes. */
void gps_receiver_set_callback(gps_fix_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __GPS_RECEIVER_H */
