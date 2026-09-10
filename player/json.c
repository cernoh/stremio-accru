/*
 * json.c — minimal JSON DOM for the stremio-accru player IPC.
 *
 * This is a compact standalone implementation of the small subset of JSON
 * the shell transport needs: string/array/object values with numbers and
 * booleans, recursive parse with escaping, compact dump with escaping.
 * It exists so the player host has no json-glib dependency.
 */
#include "json.h"

#include <math.h>
#include <string.h>

static void pair_free(gpointer p) {
  SaJsonPair *pair = p;
  g_free(pair->key);
  sa_json_free(pair->value);
  g_free(pair);
}

SaJson *sa_json_new_null(void) {
  SaJson *j = g_new0(SaJson, 1);
  j->type = SA_J_NULL;
  return j;
}

SaJson *sa_json_new_bool(gboolean b) {
  SaJson *j = g_new0(SaJson, 1);
  j->type = SA_J_BOOL;
  j->u.b = b;
  return j;
}

SaJson *sa_json_new_number(double n) {
  SaJson *j = g_new0(SaJson, 1);
  j->type = SA_J_NUMBER;
  j->u.num = n;
  return j;
}

SaJson *sa_json_new_string(const gchar *s) {
  SaJson *j = g_new0(SaJson, 1);
  j->type = SA_J_STRING;
  j->u.str = g_strdup(s ? s : "");
  return j;
}

SaJson *sa_json_new_array(void) {
  SaJson *j = g_new0(SaJson, 1);
  j->type = SA_J_ARRAY;
  j->u.arr = g_ptr_array_new_with_free_func((GDestroyNotify)sa_json_free);
  return j;
}

SaJson *sa_json_new_object(void) {
  SaJson *j = g_new0(SaJson, 1);
  j->type = SA_J_OBJECT;
  j->u.obj = g_ptr_array_new_with_free_func(pair_free);
  return j;
}

void sa_json_free(SaJson *j) {
  if (!j) return;
  switch (j->type) {
    case SA_J_STRING:
      g_free(j->u.str);
      break;
    case SA_J_ARRAY:
      g_ptr_array_free(j->u.arr, TRUE);
      break;
    case SA_J_OBJECT:
      g_ptr_array_free(j->u.obj, TRUE);
      break;
    default:
      break;
  }
  g_free(j);
}

void sa_json_array_add(SaJson *arr, SaJson *child) {
  g_ptr_array_add(arr->u.arr, child);
}

void sa_json_object_set(SaJson *obj, const gchar *key, SaJson *value) {
  for (guint i = 0; i < obj->u.obj->len; i++) {
    SaJsonPair *pair = g_ptr_array_index(obj->u.obj, i);
    if (strcmp(pair->key, key) == 0) {
      sa_json_free(pair->value);
      pair->value = value;
      return;
    }
  }
  SaJsonPair *pair = g_new0(SaJsonPair, 1);
  pair->key = g_strdup(key);
  pair->value = value;
  g_ptr_array_add(obj->u.obj, pair);
}

const SaJson *sa_json_object_get(const SaJson *obj, const gchar *key) {
  if (!obj || obj->type != SA_J_OBJECT) return NULL;
  for (guint i = 0; i < obj->u.obj->len; i++) {
    SaJsonPair *pair = g_ptr_array_index(obj->u.obj, i);
    if (strcmp(pair->key, key) == 0) return pair->value;
  }
  return NULL;
}

/* ---------------- writer ---------------- */

typedef struct {
  GString *out;
} Writer;

static void write_value(Writer *w, const SaJson *j);

static void write_string_escaped(Writer *w, const gchar *s) {
  g_string_append_c(w->out, '"');
  for (const guchar *p = (const guchar *)s; *p; p++) {
    switch (*p) {
      case '"': g_string_append(w->out, "\\\""); break;
      case '\\': g_string_append(w->out, "\\\\"); break;
      case '\b': g_string_append(w->out, "\\b"); break;
      case '\f': g_string_append(w->out, "\\f"); break;
      case '\n': g_string_append(w->out, "\\n"); break;
      case '\r': g_string_append(w->out, "\\r"); break;
      case '\t': g_string_append(w->out, "\\t"); break;
      default:
        if (*p < 0x20)
          g_string_append_printf(w->out, "\\u%04x", *p);
        else
          g_string_append_c(w->out, (gchar)*p);
    }
  }
  g_string_append_c(w->out, '"');
}

static void write_object(Writer *w, const GPtrArray *pairs) {
  g_string_append_c(w->out, '{');
  for (guint i = 0; i < pairs->len; i++) {
    if (i) g_string_append_c(w->out, ',');
    SaJsonPair *pair = g_ptr_array_index(pairs, i);
    write_string_escaped(w, pair->key);
    g_string_append_c(w->out, ':');
    write_value(w, pair->value);
  }
  g_string_append_c(w->out, '}');
}

static void write_value(Writer *w, const SaJson *j) {
  switch (j->type) {
    case SA_J_NULL:
      g_string_append(w->out, "null");
      break;
    case SA_J_BOOL:
      g_string_append(w->out, j->u.b ? "true" : "false");
      break;
    case SA_J_NUMBER: {
      double d = j->u.num;
      if (d == (double)(gint64)d && fabs(d) < 9.0e15)
        g_string_append_printf(w->out, "%lld", (long long)d);
      else
        g_string_append_printf(w->out, "%.17g", d);
      break;
    }
    case SA_J_STRING:
      write_string_escaped(w, j->u.str);
      break;
    case SA_J_ARRAY: {
      g_string_append_c(w->out, '[');
      for (guint i = 0; i < j->u.arr->len; i++) {
        if (i) g_string_append_c(w->out, ',');
        write_value(w, g_ptr_array_index(j->u.arr, i));
      }
      g_string_append_c(w->out, ']');
      break;
    }
    case SA_J_OBJECT:
      write_object(w, j->u.obj);
      break;
  }
}

gchar *sa_json_dump(const SaJson *j) {
  Writer w = {.out = g_string_new(NULL)};
  write_value(&w, j);
  return g_string_free(w.out, FALSE);
}

/* ---------------- parser ---------------- */

typedef struct {
  const gchar *p;
  const gchar *end;
  gboolean failed;
  guint depth;
} Parser;

static SaJson *parse_value(Parser *ps);

static void skip_ws(Parser *ps) {
  while (ps->p < ps->end &&
         (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r'))
    ps->p++;
}

static gboolean eat(Parser *ps, gchar c) {
  skip_ws(ps);
  if (ps->p < ps->end && *ps->p == c) {
    ps->p++;
    return TRUE;
  }
  return FALSE;
}

static guint hex4(const gchar *s) {
  guint v = 0;
  for (int i = 0; i < 4; i++) {
    gchar c = s[i];
    v <<= 4;
    if (c >= '0' && c <= '9') v |= c - '0';
    else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    else return 0xFFFF;
  }
  return v;
}

static void append_utf8(GString *out, guint32 cp) {
  if (cp < 0x80) {
    g_string_append_c(out, (gchar)cp);
  } else if (cp < 0x800) {
    g_string_append_c(out, (gchar)(0xC0 | (cp >> 6)));
    g_string_append_c(out, (gchar)(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    g_string_append_c(out, (gchar)(0xE0 | (cp >> 12)));
    g_string_append_c(out, (gchar)(0x80 | ((cp >> 6) & 0x3F)));
    g_string_append_c(out, (gchar)(0x80 | (cp & 0x3F)));
  } else {
    g_string_append_c(out, (gchar)(0xF0 | (cp >> 18)));
    g_string_append_c(out, (gchar)(0x80 | ((cp >> 12) & 0x3F)));
    g_string_append_c(out, (gchar)(0x80 | ((cp >> 6) & 0x3F)));
    g_string_append_c(out, (gchar)(0x80 | (cp & 0x3F)));
  }
}

static SaJson *parse_string(Parser *ps) {
  if (ps->p >= ps->end || *ps->p != '"') {
    ps->failed = TRUE;
    return NULL;
  }
  ps->p++;
  GString *out = g_string_new(NULL);
  while (ps->p < ps->end) {
    guchar c = (guchar)*ps->p++;
    if (c == '"') {
      SaJson *j = sa_json_new_string(out->str);
      g_string_free(out, TRUE);
      return j;
    }
    if (c == '\\') {
      if (ps->p >= ps->end) break;
      guchar e = (guchar)*ps->p++;
      switch (e) {
        case '"': g_string_append_c(out, '"'); break;
        case '\\': g_string_append_c(out, '\\'); break;
        case '/': g_string_append_c(out, '/'); break;
        case 'b': g_string_append_c(out, '\b'); break;
        case 'f': g_string_append_c(out, '\f'); break;
        case 'n': g_string_append_c(out, '\n'); break;
        case 'r': g_string_append_c(out, '\r'); break;
        case 't': g_string_append_c(out, '\t'); break;
        case 'u': {
          if (ps->end - ps->p < 4) { ps->failed = TRUE; goto err; }
          guint32 cp = hex4(ps->p);
          ps->p += 4;
          if (cp >= 0xD800 && cp <= 0xDBFF && ps->end - ps->p >= 6 &&
              ps->p[0] == '\\' && ps->p[1] == 'u') {
            guint32 lo = hex4(ps->p + 2);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
              ps->p += 6;
            }
          }
          append_utf8(out, cp);
          break;
        }
        default:
          ps->failed = TRUE;
          goto err;
      }
    } else {
      g_string_append_c(out, (gchar)c);
    }
  }
err:
  ps->failed = TRUE;
  g_string_free(out, TRUE);
  return NULL;
}

static gboolean parse_literal(Parser *ps, const gchar *lit) {
  size_t n = strlen(lit);
  if ((size_t)(ps->end - ps->p) < n || strncmp(ps->p, lit, n) != 0) {
    ps->failed = TRUE;
    return FALSE;
  }
  ps->p += n;
  return TRUE;
}

static SaJson *parse_number(Parser *ps) {
  const gchar *start = ps->p;
  if (ps->p < ps->end && *ps->p == '-') ps->p++;
  while (ps->p < ps->end && g_ascii_isdigit(*ps->p)) ps->p++;
  if (ps->p < ps->end && *ps->p == '.') {
    ps->p++;
    while (ps->p < ps->end && g_ascii_isdigit(*ps->p)) ps->p++;
  }
  if (ps->p < ps->end && (*ps->p == 'e' || *ps->p == 'E')) {
    ps->p++;
    if (ps->p < ps->end && (*ps->p == '+' || *ps->p == '-')) ps->p++;
    while (ps->p < ps->end && g_ascii_isdigit(*ps->p)) ps->p++;
  }
  gchar *tmp = g_strndup(start, ps->p - start);
  gchar *endp = NULL;
  double d = g_ascii_strtod(tmp, &endp);
  gboolean ok = endp && *endp == '\0';
  g_free(tmp);
  if (!ok) {
    ps->failed = TRUE;
    return NULL;
  }
  return sa_json_new_number(d);
}

static SaJson *parse_array(Parser *ps) {
  ps->p++; /* [ */
  SaJson *arr = sa_json_new_array();
  if (eat(ps, ']')) return arr;
  while (TRUE) {
    SaJson *v = parse_value(ps);
    if (ps->failed) { sa_json_free(arr); return NULL; }
    sa_json_array_add(arr, v);
    if (eat(ps, ']')) return arr;
    if (!eat(ps, ',')) { sa_json_free(arr); ps->failed = TRUE; return NULL; }
  }
}

static SaJson *parse_object(Parser *ps) {
  ps->p++; /* { */
  SaJson *obj = sa_json_new_object();
  if (eat(ps, '}')) return obj;
  while (TRUE) {
    skip_ws(ps);
    SaJson *k = parse_string(ps);
    if (ps->failed) { sa_json_free(obj); return NULL; }
    if (!eat(ps, ':')) { sa_json_free(k); sa_json_free(obj); ps->failed = TRUE; return NULL; }
    SaJson *v = parse_value(ps);
    if (ps->failed) { sa_json_free(k); sa_json_free(obj); return NULL; }
    sa_json_object_set(obj, k->u.str, v);
    sa_json_free(k);
    if (eat(ps, '}')) return obj;
    if (!eat(ps, ',')) { sa_json_free(obj); ps->failed = TRUE; return NULL; }
  }
}

static SaJson *parse_value(Parser *ps) {
  if (++ps->depth > 64) { ps->failed = TRUE; return NULL; }
  skip_ws(ps);
  SaJson *j = NULL;
  if (ps->p >= ps->end) {
    ps->failed = TRUE;
  } else if (*ps->p == '"') {
    j = parse_string(ps);
  } else if (*ps->p == '{') {
    j = parse_object(ps);
  } else if (*ps->p == '[') {
    j = parse_array(ps);
  } else if (*ps->p == 't') {
    if (parse_literal(ps, "true")) j = sa_json_new_bool(TRUE);
  } else if (*ps->p == 'f') {
    if (parse_literal(ps, "false")) j = sa_json_new_bool(FALSE);
  } else if (*ps->p == 'n') {
    if (parse_literal(ps, "null")) j = sa_json_new_null();
  } else if (*ps->p == '-' || g_ascii_isdigit(*ps->p)) {
    j = parse_number(ps);
  } else {
    ps->failed = TRUE;
  }
  ps->depth--;
  return j;
}

SaJson *sa_json_clone(const SaJson *j) {
  if (!j) return sa_json_new_null();
  switch (j->type) {
    case SA_J_NULL:
      return sa_json_new_null();
    case SA_J_BOOL:
      return sa_json_new_bool(j->u.b);
    case SA_J_NUMBER:
      return sa_json_new_number(j->u.num);
    case SA_J_STRING:
      return sa_json_new_string(j->u.str);
    case SA_J_ARRAY: {
      SaJson *a = sa_json_new_array();
      for (guint i = 0; i < j->u.arr->len; i++)
        sa_json_array_add(a, sa_json_clone(g_ptr_array_index(j->u.arr, i)));
      return a;
    }
    case SA_J_OBJECT: {
      SaJson *o = sa_json_new_object();
      for (guint i = 0; i < j->u.obj->len; i++) {
        SaJsonPair *pair = g_ptr_array_index(j->u.obj, i);
        sa_json_object_set(o, pair->key, sa_json_clone(pair->value));
      }
      return o;
    }
  }
  return sa_json_new_null();
}

SaJson *sa_json_parse(const gchar *text, gssize len) {
  if (!text) return NULL;
  Parser ps = {
      .p = text,
      .end = len < 0 ? text + strlen(text) : text + len,
      .failed = FALSE,
      .depth = 0,
  };
  SaJson *j = parse_value(&ps);
  if (ps.failed || !j) {
    sa_json_free(j);
    return NULL;
  }
  skip_ws(&ps);
  if (ps.p != ps.end) {
    sa_json_free(j);
    return NULL;
  }
  return j;
}
