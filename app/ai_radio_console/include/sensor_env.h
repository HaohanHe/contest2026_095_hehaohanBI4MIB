#ifndef __SENSOR_ENV_H
#define __SENSOR_ENV_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Validity flags for env_sensor_data_t.valid_flags */
#define ENV_VALID_TEMP  (1U << 0)
#define ENV_VALID_HUMI  (1U << 1)
#define ENV_VALID_LIGHT (1U << 2)
#define ENV_VALID_PROX  (1U << 3)

/**
 * Environmental sensor sample.
 *
 * The module reads SHTC3 (temperature/humidity) and LTR553 (ambient light /
 * proximity) through standard character devices.  Individual fields are marked
 * valid only when the corresponding read succeeded and the value is within a
 * sane range.
 */
typedef struct {
    float    temperature;   /*!< Temperature in degrees Celsius */
    float    humidity;      /*!< Relative humidity in %RH */
    float    light;         /*!< Ambient light in lux */
    int      proximity;     /*!< Proximity value (driver specific, 0 = far) */
    bool     proximity_near;/*!< true when proximity indicates an object near */
    uint32_t valid_flags;   /*!< Bitmask of ENV_VALID_* */
    uint32_t timestamp;     /*!< Sample time in milliseconds (CLOCK_MONOTONIC) */
} env_sensor_data_t;

/*! Initialize the environmental sensor module. Safe to call when devices are
 *  not present: the background thread will keep trying to read them. */
int sensor_env_init(void);

/*! Start the background sensor polling thread. */
int sensor_env_start(void);

/*! Stop the background sensor polling thread. */
int sensor_env_stop(void);

/*! Copy the most recent cached sensor sample into @p out. */
int sensor_env_get(env_sensor_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_ENV_H */
