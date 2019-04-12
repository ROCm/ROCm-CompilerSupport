#ifndef SERVICES_H
#define SERVICES_H

typedef enum {
    SERVICE_DEFAULT,
    SERVICE_FUNCTION_CALL
} service_id_t;

void
register_services(void);

#endif // SERVICES_H
