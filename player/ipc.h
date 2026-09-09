/*
 * ipc.h — shell <-> web UI transport (Qt-WebChannel wire contract).
 *
 * Mirrors the message shapes the current Stremio web UI uses against the
 * official Linux shell (behavior reference: Stremio/stremio-linux-shell
 * src/app/ipc). All messages are JSON strings exchanged over the webview
 * script-message channel:
 *
 *   inbound  {"type":3} or {"type":6,"args":["<event>", <data>]}
 *   outbound {"id":N,"type":1,"object":"transport","args":["<event>",<data>]}
 *   init     {"id":0,"type":3,"object":"transport","data":{"transport":
 *              {"properties":[[],["","shellVersion","",V]],"signals":[],
 *               "methods":[["onEvent"]]}}}
 */
#ifndef SA_IPC_H
#define SA_IPC_H

#include "json.h"

typedef struct {
  guint type;        /* 3 (init) or 6 (invoke) */
  gchar *name;       /* event name for type 6, NULL for type 3 */
  SaJson *data;      /* args[1] for type 6, NULL when absent */
} SaInbound;

/* Parse an inbound JSON string. Returns NULL on malformed input. */
SaInbound *sa_ipc_parse(const gchar *raw);
void sa_ipc_inbound_free(SaInbound *in);

/* Outbound init handshake (reply to inbound type 3). Caller frees. */
gchar *sa_ipc_init_message(const gchar *shell_version);
/* Outbound type-1 event with a JSON payload (takes ownership of payload on
 * success of the caller; the caller always retains ownership — payload is
 * serialized here and not consumed). */
gchar *sa_ipc_event_message(const gchar *name, const SaJson *payload);

#endif /* SA_IPC_H */
