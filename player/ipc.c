/* ipc.c — see ipc.h. */
#include "ipc.h"

SaInbound *sa_ipc_parse(const gchar *raw) {
  SaJson *root = sa_json_parse(raw, -1);
  if (!root || root->type != SA_J_OBJECT) {
    sa_json_free(root);
    return NULL;
  }
  SaInbound *in = g_new0(SaInbound, 1);
  const SaJson *t = sa_json_object_get(root, "type");
  if (!t || t->type != SA_J_NUMBER) {
    sa_ipc_inbound_free(in);
    sa_json_free(root);
    return NULL;
  }
  in->type = (guint)t->u.num;

  if (in->type == 6) {
    const SaJson *args = sa_json_object_get(root, "args");
    if (args && args->type == SA_J_ARRAY && args->u.arr->len > 0) {
      const SaJson *first = g_ptr_array_index(args->u.arr, 0);
      if (first->type == SA_J_STRING) in->name = g_strdup(first->u.str);
      if (args->u.arr->len > 1)
        in->data = sa_json_clone(g_ptr_array_index(args->u.arr, 1));
    }
  }
  sa_json_free(root);
  if (in->type == 6 && !in->name) {
    sa_ipc_inbound_free(in);
    return NULL;
  }
  return in;
}

void sa_ipc_inbound_free(SaInbound *in) {
  if (!in) return;
  g_free(in->name);
  sa_json_free(in->data);
  g_free(in);
}

gchar *sa_ipc_init_message(const gchar *shell_version) {
  /* {"id":0,"type":3,"object":"transport","data":{"transport":
   *   {"properties":[[],["","shellVersion","",<ver>]],"signals":[],
   *    "methods":[["onEvent"]]}}} */
  SaJson *props_inner = sa_json_new_array();
  sa_json_array_add(props_inner, sa_json_new_array());
  SaJson *ver = sa_json_new_array();
  sa_json_array_add(ver, sa_json_new_string(""));
  sa_json_array_add(ver, sa_json_new_string("shellVersion"));
  sa_json_array_add(ver, sa_json_new_string(""));
  sa_json_array_add(ver, sa_json_new_string(shell_version));
  sa_json_array_add(props_inner, ver);

  SaJson *methods = sa_json_new_array();
  SaJson *on_event = sa_json_new_array();
  sa_json_array_add(on_event, sa_json_new_string("onEvent"));
  sa_json_array_add(methods, on_event);

  SaJson *transport = sa_json_new_object();
  sa_json_object_set(transport, "properties", props_inner);
  sa_json_object_set(transport, "signals", sa_json_new_array());
  sa_json_object_set(transport, "methods", methods);

  SaJson *data = sa_json_new_object();
  sa_json_object_set(data, "transport", transport);

  SaJson *root = sa_json_new_object();
  sa_json_object_set(root, "id", sa_json_new_number(0));
  sa_json_object_set(root, "type", sa_json_new_number(3));
  sa_json_object_set(root, "object", sa_json_new_string("transport"));
  sa_json_object_set(root, "data", data);

  gchar *out = sa_json_dump(root);
  sa_json_free(root);
  return out;
}

gchar *sa_ipc_event_message(const gchar *name, const SaJson *payload) {
  SaJson *args = sa_json_new_array();
  sa_json_array_add(args, sa_json_new_string(name));
  sa_json_array_add(args, payload ? sa_json_clone(payload)
                                  : sa_json_new_null());
  SaJson *root = sa_json_new_object();
  sa_json_object_set(root, "id", sa_json_new_number(1));
  sa_json_object_set(root, "type", sa_json_new_number(1));
  sa_json_object_set(root, "object", sa_json_new_string("transport"));
  sa_json_object_set(root, "args", args);
  gchar *out = sa_json_dump(root);
  sa_json_free(root);
  return out;
}
