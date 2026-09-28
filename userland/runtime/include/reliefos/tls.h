#ifndef RELIEFOS_TLS_H
#define RELIEFOS_TLS_H

#include <stdint.h>

typedef int (*reliefos_tls_stream_callback)(const void *data, uint32_t length,
                                          void *context);

int reliefos_tls_http_exchange(int socket, const char *hostname,
                             uint32_t timeout_ms,
                             const void *request_headers,
                             uint32_t request_headers_len,
                             const void *request_body,
                             uint32_t request_body_len,
                              char *response, uint32_t response_capacity,
                              uint32_t *response_len);
int reliefos_tls_http_stream(int socket, const char *hostname,
                           uint32_t timeout_ms,
                           const void *request_headers,
                           uint32_t request_headers_len,
                           const void *request_body,
                           uint32_t request_body_len,
                           reliefos_tls_stream_callback callback,
                           void *context);

#endif
