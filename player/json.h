/*
 * json.h — minimal JSON DOM for the stremio-accru player IPC.
 * Small, allocation-light: parser + writer + accessors. No dependencies
 * beyond GLib. Values own their children.
 */
#ifndef SA_JSON_H
#define SA_JSON_H

#include <glib.h>

typedef enum {
  SA_J_NULL,
  SA_J_BOOL,
  SA_J_NUMBER,
  SA_J_STRING,
  SA_J_ARRAY,
  SA_J_OBJECT,
} SaJsonType;

typedef struct SaJson SaJson;

struct SaJson {
  SaJsonType type;
  union {
    gboolean b;
    double num;
    gchar *str;              /* SA_J_STRING */
    GPtrArray *arr;          /* SA_J_ARRAY  (SaJson*) */
    GPtrArray *obj;          /* SA_J_OBJECT (SaJsonPair*) */
  } u;
};

typedef struct {
  gchar *key;
  SaJson *value;
} SaJsonPair;

SaJson *sa_json_new_null(void);
SaJson *sa_json_new_bool(gboolean b);
SaJson *sa_json_new_number(double n);
SaJson *sa_json_new_string(const gchar *s);
SaJson *sa_json_new_array(void);
SaJson *sa_json_new_object(void);
void sa_json_free(SaJson *j);

void sa_json_array_add(SaJson *arr, SaJson *child);
/* Steals key and value. */
void sa_json_object_set(SaJson *obj, const gchar *key, SaJson *value);
/* Borrowed. */
const SaJson *sa_json_object_get(const SaJson *obj, const gchar *key);

/* Compact serialization. Caller frees with g_free. */
gchar *sa_json_dump(const SaJson *j);
/* Parse a complete JSON document. NULL on error. */
SaJson *sa_json_parse(const gchar *text, gssize len);
/* Deep copy. Never returns NULL for a valid node. */
SaJson *sa_json_clone(const SaJson *j);

#endif /* SA_JSON_H */
