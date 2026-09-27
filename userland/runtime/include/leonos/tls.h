/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/tls.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_TLS_H
#define LEONOS_TLS_H
#include <reliefos/tls.h>

/* Old names alias the single canonical declaration. */
#define leonos_tls_http_exchange reliefos_tls_http_exchange
#define leonos_tls_http_stream reliefos_tls_http_stream
#define leonos_tls_stream_callback reliefos_tls_stream_callback
#endif /* LEONOS_TLS_H */
