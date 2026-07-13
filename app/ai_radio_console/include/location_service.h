#ifndef __LOCATION_SERVICE_H
#define __LOCATION_SERVICE_H

#include <stdbool.h>
#include "gps_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! Initialize the location service. */
int location_service_init(void);

/*! Start the location service and the underlying GPS receiver. */
int location_service_start(void);

/*! Stop the location service and the underlying GPS receiver. */
int location_service_stop(void);

/*! Copy the latest fix to out_fix; return true if the cached fix is valid. */
bool location_service_get_fix(gps_fix_t *out_fix);

/*! Return true if a valid fix is currently cached. */
bool location_service_has_valid_fix(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOCATION_SERVICE_H */
