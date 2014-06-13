/*
   SSSD

   System Database

   Copyright (C) 2008-2011 Simo Sorce <ssorce@redhat.com>
   Copyright (C) 2008-2011 Stephen Gallagher <ssorce@redhat.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "util/util.h"
#include "util/strtonum.h"
#include "util/sss_utf8.h"
#include "db/sysdb_private.h"
#include "confdb/confdb.h"
#include <time.h>

#define LDB_MODULES_PATH "LDB_MODULES_PATH"

errno_t sysdb_ldb_connect(TALLOC_CTX *mem_ctx, const char *filename,
                          struct ldb_context **_ldb)
{
    int ret;
    struct ldb_context *ldb;
    const char *mod_path;

    if (_ldb == NULL) {
        return EINVAL;
    }

    ldb = ldb_init(mem_ctx, NULL);
    if (!ldb) {
        return EIO;
    }

    ret = ldb_set_debug(ldb, ldb_debug_messages, NULL);
    if (ret != LDB_SUCCESS) {
        return EIO;
    }

    mod_path = getenv(LDB_MODULES_PATH);
    if (mod_path != NULL) {
        DEBUG(SSSDBG_TRACE_ALL, "Setting ldb module path to [%s].\n", mod_path);
        ldb_set_modules_dir(ldb, mod_path);
    }

    ret = ldb_connect(ldb, filename, 0, NULL);
    if (ret != LDB_SUCCESS) {
        return EIO;
    }

    *_ldb = ldb;

    return EOK;
}

errno_t sysdb_dn_sanitize(TALLOC_CTX *mem_ctx, const char *input,
                          char **sanitized)
{
    struct ldb_val val;
    errno_t ret = EOK;

    val.data = (uint8_t *)talloc_strdup(mem_ctx, input);
    if (!val.data) {
        return ENOMEM;
    }

    /* We can't include the trailing NULL because it would
     * be escaped and result in an unterminated string
     */
    val.length = strlen(input);

    *sanitized = ldb_dn_escape_value(mem_ctx, val);
    if (!*sanitized) {
        ret = ENOMEM;
    }

    talloc_free(val.data);
    return ret;
}

struct ldb_dn *sysdb_custom_subtree_dn(TALLOC_CTX *mem_ctx,
                                       struct sss_domain_info *dom,
                                       const char *subtree_name)
{
    errno_t ret;
    char *clean_subtree;
    struct ldb_dn *dn = NULL;
    TALLOC_CTX *tmp_ctx;

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) return NULL;

    ret = sysdb_dn_sanitize(tmp_ctx, subtree_name, &clean_subtree);
    if (ret != EOK) {
        talloc_free(tmp_ctx);
        return NULL;
    }

    dn = ldb_dn_new_fmt(tmp_ctx, dom->sysdb->ldb, SYSDB_TMPL_CUSTOM_SUBTREE,
                        clean_subtree, dom->name);
    if (dn) {
        talloc_steal(mem_ctx, dn);
    }
    talloc_free(tmp_ctx);

    return dn;
}

struct ldb_dn *sysdb_custom_dn(TALLOC_CTX *mem_ctx,
                               struct sss_domain_info *dom,
                               const char *object_name,
                               const char *subtree_name)
{
    errno_t ret;
    TALLOC_CTX *tmp_ctx;
    char *clean_name;
    char *clean_subtree;
    struct ldb_dn *dn = NULL;

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        return NULL;
    }

    ret = sysdb_dn_sanitize(tmp_ctx, object_name, &clean_name);
    if (ret != EOK) {
        goto done;
    }

    ret = sysdb_dn_sanitize(tmp_ctx, subtree_name, &clean_subtree);
    if (ret != EOK) {
        goto done;
    }

    dn = ldb_dn_new_fmt(mem_ctx, dom->sysdb->ldb, SYSDB_TMPL_CUSTOM, clean_name,
                        clean_subtree, dom->name);

done:
    talloc_free(tmp_ctx);
    return dn;
}

struct ldb_dn *sysdb_user_dn(TALLOC_CTX *mem_ctx, struct sss_domain_info *dom,
                             const char *name)
{
    errno_t ret;
    char *clean_name;
    struct ldb_dn *dn;

    ret = sysdb_dn_sanitize(NULL, name, &clean_name);
    if (ret != EOK) {
        return NULL;
    }

    dn = ldb_dn_new_fmt(mem_ctx, dom->sysdb->ldb, SYSDB_TMPL_USER,
                        clean_name, dom->name);
    talloc_free(clean_name);

    return dn;
}

struct ldb_dn *sysdb_group_dn(TALLOC_CTX *mem_ctx,
                              struct sss_domain_info *dom, const char *name)
{
    errno_t ret;
    char *clean_name;
    struct ldb_dn *dn;

    ret = sysdb_dn_sanitize(NULL, name, &clean_name);
    if (ret != EOK) {
        return NULL;
    }

    dn = ldb_dn_new_fmt(mem_ctx, dom->sysdb->ldb, SYSDB_TMPL_GROUP,
                        clean_name, dom->name);
    talloc_free(clean_name);

    return dn;
}

struct ldb_dn *sysdb_netgroup_dn(TALLOC_CTX *mem_ctx,
                                 struct sss_domain_info *dom, const char *name)
{
    errno_t ret;
    char *clean_name;
    struct ldb_dn *dn;

    ret = sysdb_dn_sanitize(NULL, name, &clean_name);
    if (ret != EOK) {
        return NULL;
    }

    dn = ldb_dn_new_fmt(mem_ctx, dom->sysdb->ldb, SYSDB_TMPL_NETGROUP,
                        clean_name, dom->name);
    talloc_free(clean_name);

    return dn;
}

struct ldb_dn *sysdb_netgroup_base_dn(TALLOC_CTX *mem_ctx,
                                      struct sss_domain_info *dom)
{
    return ldb_dn_new_fmt(mem_ctx, dom->sysdb->ldb,
                          SYSDB_TMPL_NETGROUP_BASE, dom->name);
}

errno_t sysdb_get_rdn(struct sysdb_ctx *sysdb, TALLOC_CTX *mem_ctx,
                      const char *dn, char **_name, char **_val)
{
    errno_t ret;
    struct ldb_dn *ldb_dn;
    const char *attr_name = NULL;
    const struct ldb_val *val;
    TALLOC_CTX *tmp_ctx;

    /* We have to create a tmp_ctx here because
     * ldb_dn_new_fmt() fails if mem_ctx is NULL
     */
    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    ldb_dn = ldb_dn_new_fmt(tmp_ctx, sysdb->ldb, "%s", dn);
    if (ldb_dn == NULL) {
        ret = ENOMEM;
        goto done;
    }

    if (_name) {
        attr_name = ldb_dn_get_rdn_name(ldb_dn);
        if (attr_name == NULL) {
            ret = EINVAL;
            goto done;
        }

        *_name = talloc_strdup(mem_ctx, attr_name);
        if (!*_name) {
            ret = ENOMEM;
            goto done;
        }
    }

    val = ldb_dn_get_rdn_val(ldb_dn);
    if (val == NULL) {
        ret = EINVAL;
        if (_name) talloc_free(*_name);
        goto done;
    }

    *_val = talloc_strndup(mem_ctx, (char *) val->data, val->length);
    if (!*_val) {
        ret = ENOMEM;
        if (_name) talloc_free(*_name);
        goto done;
    }

    ret = EOK;

done:
    talloc_zfree(tmp_ctx);
    return ret;
}

errno_t sysdb_group_dn_name(struct sysdb_ctx *sysdb, TALLOC_CTX *mem_ctx,
                            const char *dn, char **_name)
{
    return sysdb_get_rdn(sysdb, mem_ctx, dn, NULL, _name);
}

struct ldb_dn *sysdb_domain_dn(TALLOC_CTX *mem_ctx,
                               struct sss_domain_info *dom)
{
    return ldb_dn_new_fmt(mem_ctx, dom->sysdb->ldb, SYSDB_DOM_BASE, dom->name);
}

struct ldb_dn *sysdb_base_dn(struct sysdb_ctx *sysdb, TALLOC_CTX *mem_ctx)
{
    return ldb_dn_new(mem_ctx, sysdb->ldb, SYSDB_BASE);
}

struct ldb_context *sysdb_ctx_get_ldb(struct sysdb_ctx *sysdb)
{
    return sysdb->ldb;
}

struct sysdb_attrs *sysdb_new_attrs(TALLOC_CTX *mem_ctx)
{
    return talloc_zero(mem_ctx, struct sysdb_attrs);
}

int sysdb_attrs_get_el_ext(struct sysdb_attrs *attrs, const char *name,
                           bool alloc, struct ldb_message_element **el)
{
    struct ldb_message_element *e = NULL;
    int i;

    for (i = 0; i < attrs->num; i++) {
        if (strcasecmp(name, attrs->a[i].name) == 0)
            e = &(attrs->a[i]);
    }

    if (!e && alloc) {
        e = talloc_realloc(attrs, attrs->a,
                           struct ldb_message_element, attrs->num+1);
        if (!e) return ENOMEM;
        attrs->a = e;

        e[attrs->num].name = talloc_strdup(e, name);
        if (!e[attrs->num].name) return ENOMEM;

        e[attrs->num].num_values = 0;
        e[attrs->num].values = NULL;
        e[attrs->num].flags = 0;

        e = &(attrs->a[attrs->num]);
        attrs->num++;
    }

    if (!e) {
        return ENOENT;
    }

    *el = e;

    return EOK;
}

int sysdb_attrs_get_el(struct sysdb_attrs *attrs, const char *name,
                       struct ldb_message_element **el)
{
    return sysdb_attrs_get_el_ext(attrs, name, true, el);
}

int sysdb_attrs_get_string(struct sysdb_attrs *attrs, const char *name,
                           const char **string)
{
    struct ldb_message_element *el;
    int ret;

    ret = sysdb_attrs_get_el_ext(attrs, name, false, &el);
    if (ret) {
        return ret;
    }

    if (el->num_values != 1) {
        return ERANGE;
    }

    *string = (const char *)el->values[0].data;
    return EOK;
}

int sysdb_attrs_get_int32_t(struct sysdb_attrs *attrs, const char *name,
                             int32_t *value)
{
    struct ldb_message_element *el;
    int ret;
    char *endptr;
    int32_t val;

    ret = sysdb_attrs_get_el_ext(attrs, name, false, &el);
    if (ret) {
        return ret;
    }

    if (el->num_values != 1) {
        return ERANGE;
    }

    errno = 0;
    val = strtoint32((const char *) el->values[0].data, &endptr, 10);
    if (errno != 0) return errno;
    if (*endptr) return EINVAL;

    *value = val;
    return EOK;
}

int sysdb_attrs_get_uint32_t(struct sysdb_attrs *attrs, const char *name,
                             uint32_t *value)
{
    struct ldb_message_element *el;
    int ret;
    char *endptr;
    uint32_t val;

    ret = sysdb_attrs_get_el_ext(attrs, name, false, &el);
    if (ret) {
        return ret;
    }

    if (el->num_values != 1) {
        return ERANGE;
    }

    errno = 0;
    val = strtouint32((const char *) el->values[0].data, &endptr, 10);
    if (errno != 0) return errno;
    if (*endptr) return EINVAL;

    *value = val;
    return EOK;
}

int sysdb_attrs_get_uint16_t(struct sysdb_attrs *attrs, const char *name,
                             uint16_t *value)
{
    struct ldb_message_element *el;
    int ret;
    char *endptr;
    uint16_t val;

    ret = sysdb_attrs_get_el_ext(attrs, name, false, &el);
    if (ret) {
        return ret;
    }

    if (el->num_values != 1) {
        return ERANGE;
    }

    errno = 0;
    val = strtouint16((const char *) el->values[0].data, &endptr, 10);
    if (errno != 0) return errno;
    if (*endptr) return EINVAL;

    *value = val;
    return EOK;
}

errno_t sysdb_attrs_get_bool(struct sysdb_attrs *attrs, const char *name,
                             bool *value)
{
    struct ldb_message_element *el;
    int ret;

    ret = sysdb_attrs_get_el_ext(attrs, name, false, &el);
    if (ret) {
        return ret;
    }

    if (el->num_values != 1) {
        return ERANGE;
    }

    if (strcmp((const char *)el->values[0].data, "TRUE") == 0)
        *value = true;
    else
        *value = false;
    return EOK;
}

const char **sss_ldb_el_to_string_list(TALLOC_CTX *mem_ctx,
                                       struct ldb_message_element *el)
{
    unsigned int u;
    const char **a;

    a = talloc_zero_array(mem_ctx, const char *, el->num_values + 1);
    if (a == NULL) {
        return NULL;
    }

    for (u = 0; u < el->num_values; u++) {
        a[u] = talloc_strndup(a, (const char *)el->values[u].data,
                              el->values[u].length);
        if (a[u] == NULL) {
            talloc_free(a);
            return NULL;
        }
    }

    return a;
}

int sysdb_attrs_get_string_array(struct sysdb_attrs *attrs, const char *name,
                                 TALLOC_CTX *mem_ctx, const char ***string)
{
    struct ldb_message_element *el;
    int ret;
    const char **a;

    ret = sysdb_attrs_get_el_ext(attrs, name, false, &el);
    if (ret) {
        return ret;
    }

    a = sss_ldb_el_to_string_list(mem_ctx, el);
    if (a == NULL) {
        return ENOMEM;
    }

    *string = a;
    return EOK;
}

int sysdb_attrs_add_val(struct sysdb_attrs *attrs,
                        const char *name, const struct ldb_val *val)
{
    struct ldb_message_element *el = NULL;
    struct ldb_val *vals;
    int ret;

    ret = sysdb_attrs_get_el(attrs, name, &el);
    if (ret != EOK) {
        return ret;
    }

    vals = talloc_realloc(attrs->a, el->values,
                          struct ldb_val, el->num_values+1);
    if (!vals) return ENOMEM;

    vals[el->num_values] = ldb_val_dup(vals, val);
    if (vals[el->num_values].data == NULL &&
        vals[el->num_values].length != 0) {
        return ENOMEM;
    }

    el->values = vals;
    el->num_values++;

    return EOK;
}

int sysdb_attrs_add_string(struct sysdb_attrs *attrs,
                           const char *name, const char *str)
{
    struct ldb_val v;

    v.data = (uint8_t *)discard_const(str);
    v.length = strlen(str);

    return sysdb_attrs_add_val(attrs, name, &v);
}

int sysdb_attrs_add_lower_case_string(struct sysdb_attrs *attrs,
                                      const char *name, const char *str)
{
    char *lc_str;
    int ret;

    if (attrs == NULL || str == NULL) {
        return EINVAL;
    }

    lc_str = sss_tc_utf8_str_tolower(attrs, str);
    if (lc_str == NULL) {
        DEBUG(SSSDBG_OP_FAILURE, "Cannot convert name to lowercase.\n");
        return ENOMEM;
    }

    ret = sysdb_attrs_add_string(attrs, name, lc_str);
    talloc_free(lc_str);

    return ret;
}

int sysdb_attrs_add_mem(struct sysdb_attrs *attrs, const char *name,
                        const void *mem, size_t size)
{
	struct ldb_val v;

	v.data   = discard_const(mem);
	v.length = size;
	return sysdb_attrs_add_val(attrs, name, &v);
}

int sysdb_attrs_add_bool(struct sysdb_attrs *attrs,
                         const char *name, bool value)
{
    if(value) {
        return sysdb_attrs_add_string(attrs, name, "TRUE");
    }

    return sysdb_attrs_add_string(attrs, name, "FALSE");
}

int sysdb_attrs_steal_string(struct sysdb_attrs *attrs,
                             const char *name, char *str)
{
    struct ldb_message_element *el = NULL;
    struct ldb_val *vals;
    int ret;

    ret = sysdb_attrs_get_el(attrs, name, &el);
    if (ret != EOK) {
        return ret;
    }

    vals = talloc_realloc(attrs->a, el->values,
                          struct ldb_val, el->num_values+1);
    if (!vals) return ENOMEM;
    el->values = vals;

    /* now steal and assign the string */
    talloc_steal(el->values, str);

    el->values[el->num_values].data = (uint8_t *)str;
    el->values[el->num_values].length = strlen(str);
    el->num_values++;

    return EOK;
}

int sysdb_attrs_add_long(struct sysdb_attrs *attrs,
                         const char *name, long value)
{
    struct ldb_val v;
    char *str;
    int ret;

    str = talloc_asprintf(attrs, "%ld", value);
    if (!str) return ENOMEM;

    v.data = (uint8_t *)str;
    v.length = strlen(str);

    ret = sysdb_attrs_add_val(attrs, name, &v);
    talloc_free(str);

    return ret;
}

int sysdb_attrs_add_uint32(struct sysdb_attrs *attrs,
                           const char *name, uint32_t value)
{
    unsigned long val = value;
    struct ldb_val v;
    char *str;
    int ret;

    str = talloc_asprintf(attrs, "%lu", val);
    if (!str) return ENOMEM;

    v.data = (uint8_t *)str;
    v.length = strlen(str);

    ret = sysdb_attrs_add_val(attrs, name, &v);
    talloc_free(str);

    return ret;
}

int sysdb_attrs_add_time_t(struct sysdb_attrs *attrs,
                           const char *name, time_t value)
{
    long long val = value;
    struct ldb_val v;
    char *str;
    int ret;

    str = talloc_asprintf(attrs, "%lld", val);
    if (!str) return ENOMEM;

    v.data = (uint8_t *)str;
    v.length = strlen(str);

    ret = sysdb_attrs_add_val(attrs, name, &v);
    talloc_free(str);

    return ret;
}

int sysdb_attrs_add_lc_name_alias(struct sysdb_attrs *attrs,
                                  const char *value)
{
    return sysdb_attrs_add_lower_case_string(attrs, SYSDB_NAME_ALIAS, value);
}

int sysdb_attrs_copy_values(struct sysdb_attrs *src,
                            struct sysdb_attrs *dst,
                            const char *name)
{
    int ret = EOK;
    int i;
    struct ldb_message_element *src_el;

    ret = sysdb_attrs_get_el(src, name, &src_el);
    if (ret != EOK) {
        goto done;
    }

    for (i = 0; i < src_el->num_values; i++) {
        ret = sysdb_attrs_add_val(dst, name, &src_el->values[i]);
        if (ret != EOK) {
            goto done;
        }
    }

done:
    return ret;
}

int sysdb_attrs_users_from_str_list(struct sysdb_attrs *attrs,
                                    const char *attr_name,
                                    const char *domain,
                                    const char *const *list)
{
    struct ldb_message_element *el = NULL;
    struct ldb_val *vals;
    int i, j, num;
    char *member;
    int ret;

    ret = sysdb_attrs_get_el(attrs, attr_name, &el);
    if (ret) {
        return ret;
    }

    for (num = 0; list[num]; num++) /* count */ ;

    vals = talloc_realloc(attrs->a, el->values,
                          struct ldb_val, el->num_values + num);
    if (!vals) {
        return ENOMEM;
    }
    el->values = vals;

    DEBUG(SSSDBG_TRACE_ALL, "Adding %d members to existing %d ones\n",
              num, el->num_values);

    for (i = 0, j = el->num_values; i < num; i++) {

        member = sysdb_user_strdn(el->values, domain, list[i]);
        if (!member) {
            DEBUG(SSSDBG_CONF_SETTINGS,
                  "Failed to get user dn for [%s]\n", list[i]);
            continue;
        }
        el->values[j].data = (uint8_t *)member;
        el->values[j].length = strlen(member);
        j++;

        DEBUG(SSSDBG_TRACE_LIBS, "    member #%d: [%s]\n", i, member);
    }
    el->num_values = j;

    return EOK;
}

static char *build_dom_dn_str_escape(TALLOC_CTX *mem_ctx, const char *template,
                                     const char *domain, const char *name)
{
    char *ret;
    int l;

    l = strcspn(name, ",=\n+<>#;\\\"");
    if (name[l] != '\0') {
        struct ldb_val v;
        char *tmp;

        v.data = discard_const_p(uint8_t, name);
        v.length = strlen(name);

        tmp = ldb_dn_escape_value(mem_ctx, v);
        if (!tmp) {
            return NULL;
        }

        ret = talloc_asprintf(mem_ctx, template, tmp, domain);
        talloc_zfree(tmp);
        if (!ret) {
            return NULL;
        }

        return ret;
    }

    ret = talloc_asprintf(mem_ctx, template, name, domain);
    if (!ret) {
        return NULL;
    }

    return ret;
}

char *sysdb_user_strdn(TALLOC_CTX *mem_ctx,
                       const char *domain, const char *name)
{
    return build_dom_dn_str_escape(mem_ctx, SYSDB_TMPL_USER, domain, name);
}

char *sysdb_group_strdn(TALLOC_CTX *mem_ctx,
                        const char *domain, const char *name)
{
    return build_dom_dn_str_escape(mem_ctx, SYSDB_TMPL_GROUP, domain, name);
}

/* TODO: make a more complete and precise mapping */
int sysdb_error_to_errno(int ldberr)
{
    switch (ldberr) {
    case LDB_SUCCESS:
        return EOK;
    case LDB_ERR_OPERATIONS_ERROR:
        return EIO;
    case LDB_ERR_NO_SUCH_OBJECT:
        return ENOENT;
    case LDB_ERR_BUSY:
        return EBUSY;
    case LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS:
    case LDB_ERR_ENTRY_ALREADY_EXISTS:
        return EEXIST;
    case LDB_ERR_INVALID_ATTRIBUTE_SYNTAX:
        return EINVAL;
    default:
        DEBUG(SSSDBG_CRIT_FAILURE,
              "LDB returned unexpected error: [%s]\n",
               ldb_strerror(ldberr));
        return EFAULT;
    }
}

/* =Transactions========================================================== */

int sysdb_transaction_start(struct sysdb_ctx *sysdb)
{
    int ret;

    ret = ldb_transaction_start(sysdb->ldb);
    if (ret != LDB_SUCCESS) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Failed to start ldb transaction! (%d)\n", ret);
    }
    return sysdb_error_to_errno(ret);
}

int sysdb_transaction_commit(struct sysdb_ctx *sysdb)
{
    int ret;

    ret = ldb_transaction_commit(sysdb->ldb);
    if (ret != LDB_SUCCESS) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Failed to commit ldb transaction! (%d)\n", ret);
    }
    return sysdb_error_to_errno(ret);
}

int sysdb_transaction_cancel(struct sysdb_ctx *sysdb)
{
    int ret;

    ret = ldb_transaction_cancel(sysdb->ldb);
    if (ret != LDB_SUCCESS) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Failed to cancel ldb transaction! (%d)\n", ret);
    }
    return sysdb_error_to_errno(ret);
}

/* =Initialization======================================================== */

int sysdb_get_db_file(TALLOC_CTX *mem_ctx,
                      const char *provider, const char *name,
                      const char *base_path, char **_ldb_file)
{
    char *ldb_file;

    /* special case for the local domain */
    if (strcasecmp(provider, "local") == 0) {
        ldb_file = talloc_asprintf(mem_ctx, "%s/"LOCAL_SYSDB_FILE,
                                   base_path);
    } else {
        ldb_file = talloc_asprintf(mem_ctx, "%s/"CACHE_SYSDB_FILE,
                                   base_path, name);
    }
    if (!ldb_file) {
        return ENOMEM;
    }

    *_ldb_file = ldb_file;
    return EOK;
}

errno_t sysdb_domain_create(struct sysdb_ctx *sysdb, const char *domain_name)
{
    struct ldb_message *msg;
    TALLOC_CTX *tmp_ctx;
    int ret;

    tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) {
        ret = ENOMEM;
        goto done;
    }

    /* == create base domain object == */

    msg = ldb_msg_new(tmp_ctx);
    if (!msg) {
        ret = ENOMEM;
        goto done;
    }
    msg->dn = ldb_dn_new_fmt(msg, sysdb->ldb, SYSDB_DOM_BASE, domain_name);
    if (!msg->dn) {
        ret = ENOMEM;
        goto done;
    }
    ret = ldb_msg_add_string(msg, "cn", domain_name);
    if (ret != LDB_SUCCESS) {
        ret = EIO;
        goto done;
    }
    /* do a synchronous add */
    ret = ldb_add(sysdb->ldb, msg);
    if (ret != LDB_SUCCESS) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Failed to initialize DB (%d, [%s]) "
                                     "for domain %s!\n",
                                     ret, ldb_errstring(sysdb->ldb),
                                     domain_name);
        ret = EIO;
        goto done;
    }
    talloc_zfree(msg);

    /* == create Users tree == */

    msg = ldb_msg_new(tmp_ctx);
    if (!msg) {
        ret = ENOMEM;
        goto done;
    }
    msg->dn = ldb_dn_new_fmt(msg, sysdb->ldb,
                             SYSDB_TMPL_USER_BASE, domain_name);
    if (!msg->dn) {
        ret = ENOMEM;
        goto done;
    }
    ret = ldb_msg_add_string(msg, "cn", "Users");
    if (ret != LDB_SUCCESS) {
        ret = EIO;
        goto done;
    }
    /* do a synchronous add */
    ret = ldb_add(sysdb->ldb, msg);
    if (ret != LDB_SUCCESS) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Failed to initialize DB (%d, [%s]) "
                                     "for domain %s!\n",
                                     ret, ldb_errstring(sysdb->ldb),
                                     domain_name);
        ret = EIO;
        goto done;
    }
    talloc_zfree(msg);

    /* == create Groups tree == */

    msg = ldb_msg_new(tmp_ctx);
    if (!msg) {
        ret = ENOMEM;
        goto done;
    }
    msg->dn = ldb_dn_new_fmt(msg, sysdb->ldb,
                             SYSDB_TMPL_GROUP_BASE, domain_name);
    if (!msg->dn) {
        ret = ENOMEM;
        goto done;
    }
    ret = ldb_msg_add_string(msg, "cn", "Groups");
    if (ret != LDB_SUCCESS) {
        ret = EIO;
        goto done;
    }
    /* do a synchronous add */
    ret = ldb_add(sysdb->ldb, msg);
    if (ret != LDB_SUCCESS) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Failed to initialize DB (%d, [%s]) for "
                                     "domain %s!\n",
                                     ret, ldb_errstring(sysdb->ldb),
                                     domain_name);
        ret = EIO;
        goto done;
    }
    talloc_zfree(msg);

    ret = EOK;

done:
    talloc_zfree(tmp_ctx);
    return ret;
}

/* Compare versions of sysdb, returns ERRNO accordingly */
static errno_t
sysdb_version_check(const char *expected,
                    const char *received)
{
    int ret;
    unsigned int exp_major, exp_minor, recv_major, recv_minor;

    ret = sscanf(expected, "%u.%u", &exp_major, &exp_minor);
    if (ret != 2) {
        return EINVAL;
    }
    ret = sscanf(received, "%u.%u", &recv_major, &recv_minor);
    if (ret != 2) {
        return EINVAL;
    }

    if (recv_major > exp_major) {
        return EUCLEAN;
    } else if (recv_major < exp_major) {
        return EMEDIUMTYPE;
    }

    if (recv_minor > exp_minor) {
        return EUCLEAN;
    } else if (recv_minor < exp_minor) {
        return EMEDIUMTYPE;
    }

    return EOK;
}

int sysdb_domain_init_internal(TALLOC_CTX *mem_ctx,
                               struct sss_domain_info *domain,
                               const char *db_path,
                               bool allow_upgrade,
                               struct sysdb_ctx **_ctx)
{
    TALLOC_CTX *tmp_ctx = NULL;
    struct sysdb_ctx *sysdb;
    const char *base_ldif;
    struct ldb_ldif *ldif;
    struct ldb_message_element *el;
    struct ldb_result *res;
    struct ldb_dn *verdn;
    const char *version = NULL;
    int ret;

    sysdb = talloc_zero(mem_ctx, struct sysdb_ctx);
    if (!sysdb) {
        return ENOMEM;
    }

    ret = sysdb_get_db_file(sysdb, domain->provider,
                            domain->name, db_path,
                            &sysdb->ldb_file);
    if (ret != EOK) {
        goto done;
    }
    DEBUG(SSSDBG_FUNC_DATA,
          "DB File for %s: %s\n", domain->name, sysdb->ldb_file);

    ret = sysdb_ldb_connect(sysdb, sysdb->ldb_file, &sysdb->ldb);
    if (ret != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE, "sysdb_ldb_connect failed.\n");
        goto done;
    }

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        ret = ENOMEM;
        goto done;
    }

    verdn = ldb_dn_new(tmp_ctx, sysdb->ldb, SYSDB_BASE);
    if (!verdn) {
        ret = EIO;
        goto done;
    }

    ret = ldb_search(sysdb->ldb, tmp_ctx, &res,
                     verdn, LDB_SCOPE_BASE,
                     NULL, NULL);
    if (ret != LDB_SUCCESS) {
        ret = EIO;
        goto done;
    }
    if (res->count > 1) {
        ret = EIO;
        goto done;
    }

    if (res->count == 1) {
        el = ldb_msg_find_element(res->msgs[0], "version");
        if (!el) {
            ret = EIO;
            goto done;
        }

        if (el->num_values != 1) {
            ret = EINVAL;
            goto done;
        }
        version = talloc_strndup(tmp_ctx,
                                 (char *)(el->values[0].data),
                                 el->values[0].length);
        if (!version) {
            ret = ENOMEM;
            goto done;
        }

        if (strcmp(version, SYSDB_VERSION) == 0) {
            /* all fine, return */
            ret = EOK;
            goto done;
        }

        if (!allow_upgrade) {
            DEBUG(SSSDBG_FATAL_FAILURE,
                  "Wrong DB version (got %s expected %s)\n",
                   version, SYSDB_VERSION);
            ret = sysdb_version_check(SYSDB_VERSION, version);
            goto done;
        }

        DEBUG(SSSDBG_CONF_SETTINGS, "Upgrading DB [%s] from version: %s\n",
                  domain->name, version);

        if (strcmp(version, SYSDB_VERSION_0_3) == 0) {
            ret = sysdb_upgrade_03(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_4) == 0) {
            ret = sysdb_upgrade_04(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_5) == 0) {
            ret = sysdb_upgrade_05(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_6) == 0) {
            ret = sysdb_upgrade_06(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_7) == 0) {
            ret = sysdb_upgrade_07(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_8) == 0) {
            ret = sysdb_upgrade_08(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_9) == 0) {
            ret = sysdb_upgrade_09(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_10) == 0) {
            ret = sysdb_upgrade_10(sysdb, domain, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_11) == 0) {
            ret = sysdb_upgrade_11(sysdb, domain, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_12) == 0) {
            ret = sysdb_upgrade_12(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_13) == 0) {
            ret = sysdb_upgrade_13(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_14) == 0) {
            ret = sysdb_upgrade_14(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        if (strcmp(version, SYSDB_VERSION_0_15) == 0) {
            ret = sysdb_upgrade_15(sysdb, &version);
            if (ret != EOK) {
                goto done;
            }
        }

        /* The version should now match SYSDB_VERSION.
         * If not, it means we didn't match any of the
         * known older versions. The DB might be
         * corrupt or generated by a newer version of
         * SSSD.
         */
        if (strcmp(version, SYSDB_VERSION) == 0) {
            /* The cache has been upgraded.
             * We need to reopen the LDB to ensure that
             * any changes made above take effect.
             */
            talloc_zfree(sysdb->ldb);
            ret = sysdb_ldb_connect(sysdb, sysdb->ldb_file, &sysdb->ldb);
            if (ret != EOK) {
                DEBUG(SSSDBG_CRIT_FAILURE, "sysdb_ldb_connect failed.\n");
            }
            goto done;
        }

        DEBUG(SSSDBG_FATAL_FAILURE,
              "Unknown DB version [%s], expected [%s] for domain %s!\n",
                 version?version:"not found", SYSDB_VERSION, domain->name);
        ret = sysdb_version_check(SYSDB_VERSION, version);
        goto done;
    }

    /* SYSDB_BASE does not exists, means db is empty, populate */

    base_ldif = SYSDB_BASE_LDIF;
    while ((ldif = ldb_ldif_read_string(sysdb->ldb, &base_ldif))) {
        ret = ldb_add(sysdb->ldb, ldif->msg);
        if (ret != LDB_SUCCESS) {
            DEBUG(SSSDBG_FATAL_FAILURE,
                  "Failed to initialize DB (%d, [%s]) for domain %s!\n",
                      ret, ldb_errstring(sysdb->ldb), domain->name);
            ret = EIO;
            goto done;
        }
        ldb_ldif_read_free(sysdb->ldb, ldif);
    }

    ret = sysdb_domain_create(sysdb, domain->name);
    if (ret != EOK) {
        goto done;
    }

    /* The cache has been newly created.
     * We need to reopen the LDB to ensure that
     * all of the special values take effect
     * (such as enabling the memberOf plugin and
     * the various indexes).
     */
    talloc_zfree(sysdb->ldb);
    ret = sysdb_ldb_connect(sysdb, sysdb->ldb_file, &sysdb->ldb);
    if (ret != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE, "sysdb_ldb_connect failed.\n");
    }

done:
    talloc_free(tmp_ctx);
    if (ret == EOK) {
        *_ctx = sysdb;
    } else {
        talloc_free(sysdb);
    }
    return ret;
}

int sysdb_init(TALLOC_CTX *mem_ctx,
               struct sss_domain_info *domains,
               bool allow_upgrade)
{
    struct sss_domain_info *dom;
    struct sysdb_ctx *sysdb;
    int ret;

    if (allow_upgrade) {
        /* check if we have an old sssd.ldb to upgrade */
        ret = sysdb_check_upgrade_02(domains, DB_PATH);
        if (ret != EOK) {
            return ret;
        }
    }

    /* open a db for each domain */
    for (dom = domains; dom; dom = dom->next) {

        ret = sysdb_domain_init_internal(mem_ctx, dom, DB_PATH,
                                         allow_upgrade, &sysdb);
        if (ret != EOK) {
            return ret;
        }

        dom->sysdb = talloc_move(dom, &sysdb);
    }

    return EOK;
}

int sysdb_domain_init(TALLOC_CTX *mem_ctx,
                      struct sss_domain_info *domain,
                      const char *db_path,
                      struct sysdb_ctx **_ctx)
{
    return sysdb_domain_init_internal(mem_ctx, domain,
                                      db_path, false, _ctx);
}

int compare_ldb_dn_comp_num(const void *m1, const void *m2)
{
    struct ldb_message *msg1 = talloc_get_type(*(void **) discard_const(m1),
                                               struct ldb_message);
    struct ldb_message *msg2 = talloc_get_type(*(void **) discard_const(m2),
                                               struct ldb_message);

    return ldb_dn_get_comp_num(msg2->dn) - ldb_dn_get_comp_num(msg1->dn);
}

int sysdb_attrs_replace_name(struct sysdb_attrs *attrs, const char *oldname,
                             const char *newname)
{
    struct ldb_message_element *e = NULL;
    int i;
    const char *dummy;

    if (attrs == NULL || oldname == NULL || newname == NULL) return EINVAL;

    for (i = 0; i < attrs->num; i++) {
        if (strcasecmp(oldname, attrs->a[i].name) == 0) {
            e = &(attrs->a[i]);
        }
        if (strcasecmp(newname, attrs->a[i].name) == 0) {
            DEBUG(SSSDBG_MINOR_FAILURE,
                  "New attribute name [%s] already exists.\n", newname);
            return EEXIST;
        }
    }

    if (e != NULL) {
        dummy = talloc_strdup(attrs, newname);
        if (dummy == NULL) {
            DEBUG(SSSDBG_CRIT_FAILURE, "talloc_strdup failed.\n");
            return ENOMEM;
        }

        talloc_free(discard_const(e->name));
        e->name = dummy;
    }

    return EOK;
}

/* Search for all incidences of attr_name in a list of
 * sysdb_attrs and add their value to a list
 *
 * TODO: Currently only works for single-valued
 * attributes. Multi-valued attributes will return
 * only the first entry
 */
errno_t sysdb_attrs_to_list(TALLOC_CTX *mem_ctx,
                            struct sysdb_attrs **attrs,
                            int attr_count,
                            const char *attr_name,
                            char ***_list)
{
    int attr_idx;
    int i;
    char **list;
    char **tmp_list;
    int list_idx;

    *_list = NULL;

    /* Assume that every attrs entry contains the attr_name
     * This may waste a little memory if some entries don't
     * have the attribute, but it will save us the trouble
     * of continuously resizing the array.
     */
    list = talloc_array(mem_ctx, char *, attr_count+1);
    if (!list) {
        return ENOMEM;
    }

    list_idx = 0;
    /* Loop through all entries in attrs */
    for (attr_idx = 0; attr_idx < attr_count; attr_idx++) {
        /* Examine each attribute within the entry */
        for (i = 0; i < attrs[attr_idx]->num; i++) {
            if (strcasecmp(attrs[attr_idx]->a[i].name, attr_name) == 0) {
                /* Attribute name matches the requested name
                 * Copy it to the output list
                 */
                list[list_idx] = talloc_strdup(
                        list,
                        (const char *)attrs[attr_idx]->a[i].values[0].data);
                if (!list[list_idx]) {
                    talloc_free(list);
                    return ENOMEM;
                }
                list_idx++;

                /* We only support single-valued attributes
                 * Break here and go on to the next entry
                 */
                break;
            }
        }
    }

    list[list_idx] = NULL;

    /* if list_idx < attr_count, do a realloc to
     * reclaim unused memory
     */
    if (list_idx < attr_count) {
        tmp_list = talloc_realloc(mem_ctx, list, char *, list_idx+1);
        if (!tmp_list) {
            talloc_zfree(list);
            return ENOMEM;
        }
        list = tmp_list;
    }

    *_list = list;
    return EOK;
}

errno_t sysdb_get_bool(struct sysdb_ctx *sysdb,
                       struct ldb_dn *dn,
                       const char *attr_name,
                       bool *value)
{
    TALLOC_CTX *tmp_ctx;
    struct ldb_result *res;
    errno_t ret;
    int lret;
    const char *attrs[2] = {attr_name, NULL};

    tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) {
        return ENOMEM;
    }

    lret = ldb_search(sysdb->ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE,
                      attrs, NULL);
    if (lret != LDB_SUCCESS) {
        ret = sysdb_error_to_errno(lret);
        goto done;
    }

    if (res->count == 0) {
        /* This entry has not been populated in LDB
         * This is a common case, as unlike LDAP,
         * LDB does not need to have all of its parent
         * objects actually exist.
         * This object in the sysdb exists mostly just
         * to contain this attribute.
         */
        *value = false;
        ret = EOK;
        goto done;
    } else if (res->count != 1) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Got more than one reply for base search!\n");
        ret = EIO;
        goto done;
    }

    *value = ldb_msg_find_attr_as_bool(res->msgs[0], attr_name, false);

    ret = EOK;

done:
    talloc_free(tmp_ctx);
    return ret;
}

errno_t sysdb_set_bool(struct sysdb_ctx *sysdb,
                       struct ldb_dn *dn,
                       const char *cn_value,
                       const char *attr_name,
                       bool value)
{
    TALLOC_CTX *tmp_ctx = NULL;
    struct ldb_message *msg = NULL;
    struct ldb_result *res = NULL;
    errno_t ret;
    int lret;

    if (dn == NULL || cn_value == NULL || attr_name == NULL) {
        return EINVAL;
    }

    tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) {
        return ENOMEM;
    }

    lret = ldb_search(sysdb->ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE,
                      NULL, NULL);
    if (lret != LDB_SUCCESS) {
        ret = sysdb_error_to_errno(lret);
        goto done;
    }

    msg = ldb_msg_new(tmp_ctx);
    if (msg == NULL) {
        ret = ENOMEM;
        goto done;
    }
    msg->dn = dn;

    if (res->count == 0) {
        lret = ldb_msg_add_string(msg, "cn", cn_value);
        if (lret != LDB_SUCCESS) {
            ret = sysdb_error_to_errno(lret);
            goto done;
        }
    } else if (res->count != 1) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Got more than one reply for base search!\n");
        ret = EIO;
        goto done;
    } else {
        lret = ldb_msg_add_empty(msg, attr_name, LDB_FLAG_MOD_REPLACE, NULL);
        if (lret != LDB_SUCCESS) {
            ret = sysdb_error_to_errno(lret);
            goto done;
        }
    }

    lret = ldb_msg_add_string(msg, attr_name, value ? "TRUE" : "FALSE");
    if (lret != LDB_SUCCESS) {
        ret = sysdb_error_to_errno(lret);
        goto done;
    }

    if (res->count) {
        lret = ldb_modify(sysdb->ldb, msg);
    } else {
        lret = ldb_add(sysdb->ldb, msg);
    }

    ret = sysdb_error_to_errno(lret);

done:
    talloc_free(tmp_ctx);
    return ret;
}

errno_t sysdb_has_enumerated(struct sss_domain_info *domain,
                             bool *has_enumerated)
{
    errno_t ret;
    struct ldb_dn *dn;
    TALLOC_CTX *tmp_ctx;


    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        ret = ENOMEM;
        goto done;
    }

    dn = ldb_dn_new_fmt(tmp_ctx, domain->sysdb->ldb, SYSDB_DOM_BASE,
                        domain->name);
    if (!dn) {
        ret = ENOMEM;
        goto done;
    }

    ret = sysdb_get_bool(domain->sysdb, dn, SYSDB_HAS_ENUMERATED,
                         has_enumerated);

done:
    talloc_free(tmp_ctx);
    return ret;
}

errno_t sysdb_set_enumerated(struct sss_domain_info *domain,
                             bool enumerated)
{
    errno_t ret;
    TALLOC_CTX *tmp_ctx;
    struct ldb_dn *dn;

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        ret = ENOMEM;
        goto done;
    }

    dn = ldb_dn_new_fmt(tmp_ctx, domain->sysdb->ldb, SYSDB_DOM_BASE,
                        domain->name);
    if (!dn) {
        ret = ENOMEM;
        goto done;
    }

    ret = sysdb_set_bool(domain->sysdb, dn, domain->name,
                         SYSDB_HAS_ENUMERATED, enumerated);

done:
    talloc_free(tmp_ctx);
    return ret;
}

errno_t sysdb_attrs_primary_name(struct sysdb_ctx *sysdb,
                                 struct sysdb_attrs *attrs,
                                 const char *ldap_attr,
                                 const char **_primary)
{
    errno_t ret;
    char *rdn_attr = NULL;
    char *rdn_val = NULL;
    struct ldb_message_element *sysdb_name_el;
    struct ldb_message_element *orig_dn_el;
    size_t i;
    TALLOC_CTX *tmp_ctx = NULL;

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    ret = sysdb_attrs_get_el(attrs,
                             SYSDB_NAME,
                             &sysdb_name_el);
    if (ret != EOK || sysdb_name_el->num_values == 0) {
        ret = EINVAL;
        goto done;
    }

    if (sysdb_name_el->num_values == 1) {
        /* Entry contains only one name. Just return that */
        *_primary = (const char *)sysdb_name_el->values[0].data;
        ret = EOK;
        goto done;
    }

    /* Multiple values for name. Check whether one matches the RDN */

    ret = sysdb_attrs_get_el(attrs, SYSDB_ORIG_DN, &orig_dn_el);
    if (ret) {
        goto done;
    }
    if (orig_dn_el->num_values == 0) {
        DEBUG(SSSDBG_CRIT_FAILURE, "Original DN is not available.\n");
        ret = EINVAL;
        goto done;
    } else if (orig_dn_el->num_values == 1) {
        ret = sysdb_get_rdn(sysdb, tmp_ctx,
                            (const char *) orig_dn_el->values[0].data,
                            &rdn_attr,
                            &rdn_val);
        if (ret != EOK) {
            DEBUG(SSSDBG_CRIT_FAILURE, "Could not get rdn from [%s]\n",
                      (const char *) orig_dn_el->values[0].data);
            goto done;
        }
    } else {
        DEBUG(SSSDBG_CRIT_FAILURE, "Should not have more than one origDN\n");
        ret = EINVAL;
        goto done;
    }

    /* First check whether the attribute name matches */
    DEBUG(SSSDBG_TRACE_INTERNAL, "Comparing attribute names [%s] and [%s]\n",
              rdn_attr, ldap_attr);
    if (strcasecmp(rdn_attr, ldap_attr) != 0) {
        /* Multiple entries, and the RDN attribute doesn't match.
         * We have no way of resolving this deterministically,
         * so we'll use the first value as a fallback.
         */
        DEBUG(SSSDBG_MINOR_FAILURE,
              "The entry has multiple names and the RDN attribute does "
                  "not match. Will use the first value as fallback.\n");
        *_primary = (const char *)sysdb_name_el->values[0].data;
        ret = EOK;
        goto done;
    }

    for (i = 0; i < sysdb_name_el->num_values; i++) {
        if (strcasecmp(rdn_val,
                       (const char *)sysdb_name_el->values[i].data) == 0) {
            /* This name matches the RDN. Use it */
            break;
        }
    }
    if (i < sysdb_name_el->num_values) {
        /* Match was found */
        *_primary = (const char *)sysdb_name_el->values[i].data;
    } else {
        /* If we can't match the name to the RDN, we just have to
         * throw up our hands. There's no deterministic way to
         * decide which name is correct.
         */
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Cannot save entry. Unable to determine groupname\n");
        ret = EINVAL;
        goto done;
    }

    ret = EOK;

done:
    if (ret != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "Could not determine primary name: [%d][%s]\n",
                  ret, strerror(ret));
    }
    talloc_free(tmp_ctx);
    return ret;
}

/*
 * An entity with multiple names would have multiple SYSDB_NAME attributes
 * after being translated into sysdb names using a map.
 * Given a primary name returned by sysdb_attrs_primary_name(), this function
 * returns the other SYSDB_NAME attribute values so they can be saved as
 * SYSDB_NAME_ALIAS into cache.
 *
 * If lowercase is set, all aliases are duplicated in lowercase as well.
 */
errno_t sysdb_attrs_get_aliases(TALLOC_CTX *mem_ctx,
                                struct sysdb_attrs *attrs,
                                const char *primary,
                                bool lowercase,
                                const char ***_aliases)
{
    TALLOC_CTX *tmp_ctx = NULL;
    struct ldb_message_element *sysdb_name_el;
    size_t i, j, ai;
    errno_t ret;
    const char **aliases = NULL;
    const char *name;
    char *lower;

    if (_aliases == NULL) return EINVAL;

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    ret = sysdb_attrs_get_el(attrs,
                             SYSDB_NAME,
                             &sysdb_name_el);
    if (ret != EOK || sysdb_name_el->num_values == 0) {
        ret = EINVAL;
        goto done;
    }

    aliases = talloc_array(tmp_ctx, const char *,
                           sysdb_name_el->num_values + 1);
    if (!aliases) {
        ret = ENOMEM;
        goto done;
    }

    if (lowercase) {
        DEBUG(SSSDBG_TRACE_INTERNAL,
              "Domain is case-insensitive; will add lowercased aliases\n");
    }

    ai = 0;
    for (i=0; i < sysdb_name_el->num_values; i++) {
        name = (const char *)sysdb_name_el->values[i].data;

        if (lowercase) {
            /* Domain is case-insensitive. Save the lower-cased version */
            lower = sss_tc_utf8_str_tolower(tmp_ctx, name);
            if (!lower) {
                ret = ENOMEM;
                goto done;
            }

            for (j=0; j < ai; j++) {
                if (sss_utf8_case_eq((const uint8_t *) aliases[j],
                                     (const uint8_t *) lower) == ENOMATCH) {
                    break;
                }
            }

            if (ai == 0 || j < ai) {
                aliases[ai] = talloc_strdup(aliases, lower);
                if (!aliases[ai]) {
                    ret = ENOMEM;
                    goto done;
                }
                ai++;
            }
        } else {
            /* Domain is case-sensitive. Save it as-is */
            if (strcmp(primary, name) != 0) {
                aliases[ai] = talloc_strdup(aliases, name);
                if (!aliases[ai]) {
                    ret = ENOMEM;
                    goto done;
                }
                ai++;
            }
        }
    }

    aliases[ai] = NULL;

    ret = EOK;

done:
    *_aliases = talloc_steal(mem_ctx, aliases);
    talloc_free(tmp_ctx);
    return ret;
}

errno_t sysdb_attrs_primary_name_list(struct sysdb_ctx *sysdb,
                                      TALLOC_CTX *mem_ctx,
                                      struct sysdb_attrs **attr_list,
                                      size_t attr_count,
                                      const char *ldap_attr,
                                      char ***name_list)
{
    errno_t ret;
    size_t i, j;
    char **list;
    const char *name;

    /* Assume that every entry has a primary name */
    list = talloc_array(mem_ctx, char *, attr_count+1);
    if (!list) {
        return ENOMEM;
    }

    j = 0;
    for (i = 0; i < attr_count; i++) {
        ret = sysdb_attrs_primary_name(sysdb,
                                       attr_list[i],
                                       ldap_attr,
                                       &name);
        if (ret != EOK) {
            DEBUG(SSSDBG_CRIT_FAILURE, "Could not determine primary name\n");
            /* Skip and continue. Don't advance 'j' */
            continue;
        }

        list[j] = talloc_strdup(list, name);
        if (!list[j]) {
            ret = ENOMEM;
            goto done;
        }

        j++;
    }

    /* NULL-terminate the list */
    list[j] = NULL;

    *name_list = list;

    ret = EOK;

done:
    if (ret != EOK) {
        talloc_free(list);
    }
    return ret;
}

errno_t sysdb_get_real_name(TALLOC_CTX *mem_ctx,
                            struct sss_domain_info *domain,
                            const char *name,
                            const char **_cname)
{
    errno_t ret;
    TALLOC_CTX *tmp_ctx;
    struct ldb_result *res;
    const char *cname;

    tmp_ctx = talloc_new(NULL);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    ret = sysdb_getpwnam(tmp_ctx, domain, name, &res);
    if (ret != EOK) {
        DEBUG(SSSDBG_OP_FAILURE, "Cannot canonicalize username\n");
        goto done;
    }

    if (res->count == 0) {
        /* User is not cached yet */
        ret = ENOENT;
        goto done;
    } else if (res->count != 1) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "sysdb_getpwnam returned count: [%d]\n", res->count);
        ret = EIO;
        goto done;
    }

    cname = ldb_msg_find_attr_as_string(res->msgs[0], SYSDB_NAME, NULL);
    if (!cname) {
        DEBUG(SSSDBG_CRIT_FAILURE, "A user with no name?\n");
        ret = ENOENT;
        goto done;
    }

    ret = EOK;
    *_cname = talloc_steal(mem_ctx, cname);
done:
    talloc_free(tmp_ctx);
    return ret;
}

errno_t sysdb_msg2attrs(TALLOC_CTX *mem_ctx, size_t count,
                        struct ldb_message **msgs,
                        struct sysdb_attrs ***attrs)
{
    int i;
    struct sysdb_attrs **a;

    a = talloc_array(mem_ctx, struct sysdb_attrs *, count);
    if (a == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "talloc_array failed.\n");
        return ENOMEM;
    }

    for (i = 0; i < count; i++) {
        a[i] = talloc(a, struct sysdb_attrs);
        if (a[i] == NULL) {
            DEBUG(SSSDBG_CRIT_FAILURE, "talloc failed.\n");
            talloc_free(a);
            return ENOMEM;
        }
        a[i]->num = msgs[i]->num_elements;
        a[i]->a = talloc_steal(a[i], msgs[i]->elements);
    }

    *attrs = a;

    return EOK;
}
