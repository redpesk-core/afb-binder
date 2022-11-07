/*
 * Copyright (C) 2015-2021 IoT.bzh Company
 * Author "Fulup Ar Foll"
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/limits.h>

#include <rp-utils/rp-jsonc.h>

#include <libafb/afb-core.h>
#include <libafb/afb-apis.h>
#include <libafb/afb-misc.h>
#include <libafb/afb-sys.h>
#include <libafb/afb-utils.h>
#include <libafb/afb-extend.h>
#include <libafb/afb-http.h>

#include "afb-binder.h"

/* default settings */
#define AFB_HSRV_OK                 1
#define DEFAULT_API_TIMEOUT         180
#define DEFAULT_SESSION_TIMEOUT     32000000
#define DEFAULT_CACHE_TIMEOUT       100000
#define DEFAULT_MAX_SESSION_COUNT   200
#define DEFAULT_HTTP_PORT           1234
#define DEFAULT_BINDER_INTERFACE    "*"
#define DEFAULT_JOBS_MAX            200
#define DEFAULT_JOBS_MIN            10
#define DEFAULT_THREADS_POOL        2
#define DEFAULT_THREADS_MAX         5

// make our live simpler
typedef struct afb_hsrv afb_hsrv;
typedef struct afb_hreq afb_hreq;
typedef struct afb_session afb_session;
typedef struct afb_apiset afb_apiset;
typedef struct afb_apiset afb_apiset;
typedef struct afb_verb_v4 afb_verb_v4;
typedef struct afb_api_v4 afb_api_v4;
typedef struct afb_req_v4 afb_req_v4;
typedef struct afb_data afb_data;
typedef struct afb_auth afb_auth;

/**
* @brief structure for associating string and number
*/
typedef struct {
    /** the string */
    const char *label;

    /** the number */
    int  value;
}
    nsKeyEnumT;

/**
* @brief type of export
*/
typedef enum {
    /* not exported */
    AFB_EXPORT_PRIVATE=0,

    /* exported as abstract */
    AFB_EXPORT_RESTRICTED,

    /* public export */
    AFB_EXPORT_PUBLIC,
}
    ApiExportE;

/**
* @brief type of info
*/
typedef enum {
    BINDER_INFO_UID,
    BINDER_INFO_INFO,
    BINDER_INFO_PORT,
    BINDER_INFO_HTTPS,
    BINDER_INFO_ROOTDIR,
    BINDER_INFO_HTTPDIR,
}
    AfbBinderInfoE;

/**
 * @brief structure for storing permission required by verbs
 */
typedef struct {
    /** identifier for cross reference */
    const char *uid;
    /** structure recording the permission */
    afb_auth perm;
}
    afbAclsHandleT;

/**
 * @brief binder configuration deduced from JSON config
 */
typedef struct {
    /** unique id of the binder */
    const char *uid;

    /** informational text */
    const char *info;

    /** timeout of requests (seconds) */
    int timeout;

    /** verbosity */
    int verbose;

    /** global concurency setting */
    int noconcurency;

    /** url of supervision */
    const char* supervision;

    /** path of root directory */
    const char* rootdir;

    /** setting of extensions */
    json_object* extendJ;

    /** path for loading */
    json_object* ldpathJ;

    /** maximum count of threads on hold */
    int poolThreadMax;

    /** initial count of threads started */
    int poolThreadSize;

    /** maximum allowed count of pending jobs */
    int maxJobs;

    /** flags for tracing */
    struct {
        /** specification of requests tracing */
        const char* rqt;

        /** specification of events tracing */
        const char* evt;

        /** specification of apis tracing */
        const char* api;

        /** specification of globals tracing */
        const char* glob;

        /** specification of sessions tracing */
        const char* ses;
    }
        trace;

    /** specification of HTTP server */
    struct {
        /** TCP port */
        int port;

        /** */
        const char* basedir;

        /** prefix of the API */
        const char* rootapi;

        /** prefix for one page application */
        const char* onepage;

        /** upload directory */
        const char* updir;

        /** path of certificat of TLS server */
        const char* cert;

        /** path of private key for TLS server */
        const char* key;

        /** aliases specifications */
        json_object* aliasJ;

        /** */
        json_object* intfJ;

        /** timeouts */
        struct {
            /** expiration of sessions */
            int session;
            /** expiration of cache */
            int cache;
            /** expiration of requests */
            int request;
        } timeout;
    } httpd;

    /** array of permissions */
    afbAclsHandleT *acls;
}
    AfbBinderConfigT;

/**
 * @brief default binder settings
 */
static AfbBinderConfigT binderConfigDflt = {
    .uid= "any-binder",
    .timeout= DEFAULT_API_TIMEOUT,
    .rootdir=".",

    .poolThreadMax=DEFAULT_THREADS_MAX,
    .poolThreadSize=DEFAULT_THREADS_POOL,
    .maxJobs= DEFAULT_JOBS_MAX,

    .httpd.port=DEFAULT_HTTP_PORT,
    .httpd.timeout.session=DEFAULT_SESSION_TIMEOUT,
    .httpd.timeout.cache=DEFAULT_CACHE_TIMEOUT,
    .httpd.basedir=".",
    .httpd.rootapi="/api",
    .httpd.updir="/tmp",
    .httpd.onepage="/opa",
};

/**
 * @brief API configuration deduced from JSON config
 */
typedef struct {
    /** required verbosity */
    int verbose;

    /** unic identifier of the API */
    const char *uid;

    /** api name */
    const char *api;

    /** info about the api */
    const char *info;

    /** exportation status */
    ApiExportE export;

    /** concurrency status */
    const int noconcurency;

    /** provided classes */
    const char *provide;

    /** required classes */
    const char *require;

    /** description of verbs */
    json_object *verbsJ;

    /** description of events */
    json_object *eventsJ;

    /** description of aliases */
    json_object *aliasJ;

    /** sealement status */
    const int seal;

    /** import/export uri */
    const char*uri;

    /** lazyness status of clients */
    const int lazy;
}
    AfbApiConfigT;

/**
 * @brief default api settings
 */
static AfbApiConfigT apiConfigDflt= {
    .verbose=0,
    .seal=1,
};

/**
 * @brief structure for a binder
 */
struct AfbBinderHandleS {

    /** handler of HTTP service if any */
    afb_hsrv *hsrv;

    /** set of public APIs */
    afb_apiset *publicApis;

    /** set of private APIs */
    afb_apiset *privateApis;

    /** set of restricted APIs */
    afb_apiset *restrictedApis;

    /** default global API for receiving events */
    afb_api_x4_t  apiv4;

    /** configuration */
    AfbBinderConfigT config;
};

static const nsKeyEnumT afbApiExportKeys[]= {
    {"private", AFB_EXPORT_PRIVATE},
    {"restricted", AFB_EXPORT_RESTRICTED},
    {"public", AFB_EXPORT_PUBLIC},
    {NULL, -1} // terminator/on-error
};

static const nsKeyEnumT afbBinderInfoKeys[]={
    {"uid", BINDER_INFO_UID},
    {"info", BINDER_INFO_INFO},
    {"port", BINDER_INFO_PORT},
    {"https", BINDER_INFO_HTTPS},
    {"httpdir", BINDER_INFO_HTTPDIR},
    {"rootdir", BINDER_INFO_ROOTDIR},

    {NULL, -1} // terminator/on-error
};

static const nsKeyEnumT glueMagics[]= {
    {"BINDER_MAGIC_TAG",AFB_BINDER_MAGIC_TAG},
    {"API_MAGIC_TAG",AFB_API_MAGIC_TAG},
    {"RQT_MAGIC_TAG",AFB_RQT_MAGIC_TAG},
    {"EVT_MAGIC_TAG",AFB_EVT_MAGIC_TAG},
    {"TIMER_MAGIC_TAG",AFB_TIMER_MAGIC_TAG},
    {"JOB_MAGIC_TAG",AFB_JOB_MAGIC_TAG},
    {"POST_MAGIC_TAG",AFB_POST_MAGIC_TAG},
    {"CALL_MAGIC_TAG",AFB_CALL_MAGIC_TAG},

    {NULL, -1} // terminator/on-error
};

// search for key label within key/value array
static int utilLabel2Value (const nsKeyEnumT *keyvals, const char *label) {
    int idx;
    if (!label) goto OnDefaultExit;

    for (idx=0; keyvals[idx].label; idx++) {
        if (!strcasecmp (label,keyvals[ idx].label)) break;
    }
    return keyvals[idx].value;

OnDefaultExit:
    return keyvals[0].value;
}

// search for key label within key/value array
static const char* utilValue2Label (const nsKeyEnumT *keyvals, const int value) {
    const char *label=NULL;

    for (int idx=0; keyvals[idx].label; idx++) {
        if (keyvals[ idx].value == value) {
            label= keyvals[idx].label;
            break;
        }
    }
    return label;
}


/* compute the verbosity mask of the given verbosity level */
static int verbosity_to_mask(int verbosity)
{
    return (2 << (verbosity + afb_Log_Level_Error)) - 1;
}

/* Returns a string for the given magic tag value */
const char * AfbMagicToString (AfbMagicTagE magic) {
    return utilValue2Label(glueMagics, magic);
}

/**
 * @brief search an auth within API permissions' table
 *
 * @param afbAcls the table of permissions
 * @param key     the uid to search in the table
 *
 * @return the found AUTH structure if found or NULL otherwise
 */
static const afb_auth *AfbAclSearch(afbAclsHandleT *afbAcls, const char *key) {
    if (afbAcls != NULL) {
        while (afbAcls->uid != NULL) {
            if (0 == strcmp(afbAcls->uid, key))
                return &afbAcls->perm;
            afbAcls++;
        }
    }
    return NULL;
}

/* forward decl, see below */
static const char* AclBuildItem(afbAclsHandleT *acls, struct afb_auth *auth, json_object *permJ);

/**
 * @brief Put in auth the list of permissions given by permJ accordingly to the given type (afb_auth_And or afb_auth_Or)
 *
 * @param acls  array of permissions for searching cross references
 * @param auth  auth structure to fill
 * @param permJ object specifying the list of authorisations
 * @param type  type of the sequence (afb_auth_And or afb_auth_Or)
 *
 * @return NULL on success or an error report text
 */
static const char* AclBuildSeq(afbAclsHandleT *acls, struct afb_auth *auth, json_object *permJ, enum afb_auth_type type) {

    struct afb_auth *first_auth, *next_auth;
    size_t idx, count;
    const char *text;

    /* if permJ is not an array, fallback to single mode */
    if (!json_object_is_type (permJ, json_type_array))
            return AclBuildItem(acls, auth, permJ);

    /* inspect the count of items */
    count = json_object_array_length(permJ);
    if (count == 0)
        /* report the enpty array as being an error */
        return "Empty permission array";

    /* creates the chain of or/and */
    for (idx = 1 ; idx < count ; idx++) {
        /* initialisation of auth mebers with allocations */
        auth->type = type;
        auth->first = first_auth = calloc(1, sizeof *first_auth);
        auth->next = next_auth = calloc(1, sizeof *next_auth);
        if (first_auth == NULL || next_auth == NULL)
            return "out of memory";
        /* initialize the first part */
        text = AclBuildItem(acls, first_auth, json_object_array_get_idx(permJ, idx - 1));
        if (text != NULL)
            return text;
        /* initialize the next part */
        auth = next_auth;
    }
    return AclBuildItem(acls, auth, json_object_array_get_idx(permJ, count - 1));
}

/**
 * @brief Put in auth the permissions given by permJ
 *
 * @param acls  array of permissions for searching cross references
 * @param auth  auth structure to fill
 * @param permJ object specifying the authorisation
 *
 * @return NULL on success or an error report text
 */
static const char* AclBuildItem (afbAclsHandleT *acls, struct afb_auth *auth, json_object *permJ) {

    const char *text;
    json_object *val;
    struct afb_auth *first_auth;
    const struct afb_auth *other_auth;

    /* inspect the specification */
    switch (json_object_get_type(permJ)) {

    case json_type_string:
        /* a string is either a permission or a cross reference */
        text = json_object_get_string(permJ);
        if (text[0] != '#' || (other_auth = AfbAclSearch(acls, &text[1])) == NULL) {
            /* not starting with # or not found, it's a permission */
            auth->type = afb_auth_Permission;
            auth->text = text;
        }
        else {
            /* this is a reference to other_auth, trnaslate it as (other_auth OR NO) */
            auth->type = afb_auth_Or;
            auth->first = other_auth;
            /* use the last entry of acls array for setting NO */
            while (acls->uid != NULL)
                acls++;
            acls->perm.type = afb_auth_No;
            auth->next = &acls->perm;
        }
        return NULL;

    case json_type_array:
        /* arrays are implicitely reated as AND of permissions */
        return AclBuildSeq(acls, auth, permJ, afb_auth_And);

    case json_type_object:
        /* valid specs have only one key */
        if (json_object_object_length(permJ) != 1)
            break;

        /* check for keys "and" or "allOf" */
        if (json_object_object_get_ex(permJ, "and", &val)
         || json_object_object_get_ex(permJ, "AllOf", &val))
            return AclBuildSeq(acls, auth, val, afb_auth_And);

        /* check for keys "or" or "anyOf" */
        if (json_object_object_get_ex(permJ, "or", &val)
         || json_object_object_get_ex(permJ, "AnyOf", &val))
            return AclBuildSeq(acls, auth, val, afb_auth_Or);

        /* check for key "not" */
        if (json_object_object_get_ex(permJ, "not", &val)) {
            auth->type = afb_auth_Not;
            auth->first = first_auth = calloc(1, sizeof *auth->first);
            if (first_auth == NULL)
                return "out of memory";
            return AclBuildItem(acls, first_auth, val);
        }

        /* check for key "LOA" */
        if (json_object_object_get_ex(permJ, "LOA", &val)) {
            if (json_object_is_type(val, json_type_int)
                && json_object_get_int(val) >= 0
                && json_object_get_int(val) <= 7) {
                auth->type = afb_auth_LOA;
                auth->loa = (unsigned)json_object_get_int(val);
                return NULL;
            }
            return "invalid LOA value";
        }

        /* check for key "token" */
        if (json_object_object_get_ex(permJ, "token", &val)) {
            if (json_object_is_type(val, json_type_boolean)
                && json_object_get_boolean(val)) {
                auth->type = afb_auth_Token;
                return NULL;
            }
            return "invalid token value (must be true)";
        }
        break;

    default:
        break;
    }
    return json_object_get_string(permJ);
}

/* forward declaration, see below */
static void AclFreeAuthContent(afbAclsHandleT *acls, struct afb_auth *auth);

/**
 * @brief free an auth and its content if it is not in acls
 *
 * @param acls array of permissions of reference
 * @param auth the auth to be freed
 */
static void AclFreeAuth(afbAclsHandleT *acls, struct afb_auth *auth)
{
    afbAclsHandleT *it;
    for (it = acls ; auth != &it->perm ; it++) {
        if (it->uid == NULL) {
            AclFreeAuthContent(acls, auth);
            free(auth);
            break;
        }
    }
}

/**
 * @brief free the content of an Auth
 *
 * @param acls array of permissions of reference
 * @param auth the auth whose content is to be freed
 */
static void AclFreeAuthContent(afbAclsHandleT *acls, struct afb_auth *auth)
{
    switch(auth->type) {
    case afb_auth_Or:
    case afb_auth_And:
        AclFreeAuth(acls, (struct afb_auth*)auth->next);
        /*@fallthrough@*/
    case afb_auth_Not:
        AclFreeAuth(acls, (struct afb_auth*)auth->first);
        /*@fallthrough@*/
    default:
    }
}

/**
 * @brief free the memory used by acls
 *
 * @param acls the first entry of the array to free
 */
static void AfbAclFree(afbAclsHandleT *acls)
{
    afbAclsHandleT *it;
    if (acls != NULL) {
        for(it = acls ; it->uid != NULL ; it++)
            AclFreeAuthContent(acls, &it->perm);
        free(acls);
    }
}

/**
 * @brief Build an array of ACL accordingly to the given JSON spec
 *
 * @param configJ the JSON specification of ACL
 *
 * @return the computed array of permisssons or NULL on error
 */
static afbAclsHandleT* AfbAclBuildFromJsonC (json_object *configJ) {

    const char *errorMsg;
    size_t count;
    int idx;
    struct json_object_iterator it, end;
    afbAclsHandleT *acls = NULL;
    json_object *permJ;

    /* must be an object */
    if (!json_object_is_type (configJ, json_type_object)) {
        LIBAFB_ERROR ("AfbAclBuildFromJsonC:fail unexpected acl type");
        return NULL;
    }

    /* allocate the result */
    count = json_object_object_length (configJ);
    acls = calloc (count + 1, sizeof(afbAclsHandleT));
    if (acls == NULL) {
        errorMsg = "Out of memory";
        goto OnErrorExit;
    }

    /* provision keys for cross references search */
    end = json_object_iter_end(configJ);
    it = json_object_iter_begin(configJ);
    idx = 0;
    while (!json_object_iter_equal(&it, &end)) {
        acls[idx].uid = json_object_iter_peek_name(&it);
        acls[idx].perm.type = afb_auth_No;
        idx++;
        json_object_iter_next(&it);
    }

    /* build the values */
    it = json_object_iter_begin(configJ);
    idx = 0;
    while (!json_object_iter_equal(&it, &end)) {
        permJ = json_object_iter_peek_value(&it);
        errorMsg = AclBuildItem(acls, &acls[idx].perm, permJ);
        if (errorMsg) goto OnErrorExit;
        idx++;
        json_object_iter_next(&it);
    }

    /* do not delete configJ for keeping uids */
    return acls;

OnErrorExit:
    AfbAclFree(acls);
    LIBAFB_ERROR ("AfbAclBuildFromJsonC:fail acl=%s", errorMsg);
    return NULL;
}

/**
 * @brief callback for adding HTTP aliases called through rp_jsonc_optarray_until
 *
 * @param context the binder
 * @param aliasJ  the item of the array
 * @return 0 to continue or not zero to stop
 */
static int BinderAddOneAlias (void *context, json_object *aliasJ) {
    AfbBinderHandleT *binder = (AfbBinderHandleT*)context;
    const char *fullpath;
    const char *alias= json_object_get_string(aliasJ);
    const char *colon = strchr(alias, ':');
    char *prefix;
    int status;

    // split alias string
    if (colon == NULL) {
        LIBAFB_ERROR("BinderAddOneAlias Missing ':' [%s] ignored.", alias);
        goto OnErrorExit;
    }

    /* allocate copy the prefix */
    prefix = strndup(alias, colon - alias);
    if (prefix == NULL) {
        LIBAFB_ERROR("BinderAddOneAlias out of memory [%s] ignored.", alias);
        goto OnErrorExit;
    }
    fullpath= &colon[1];

    /* add the alias */
    status= afb_hsrv_add_alias(binder->hsrv, prefix, afb_common_rootdir_get_fd(), fullpath, 0, 0);
    if (status != AFB_HSRV_OK) {
        LIBAFB_ERROR("BinderAddOneAlias fail to add alias=[%s] path=[%s]", prefix, fullpath);
        free(prefix);
        goto OnErrorExit;
    }
    LIBAFB_DEBUG ("BinderAddOneAlias alias=[%s] path=[%s]", prefix, fullpath);
    return 0;

OnErrorExit:
    return -1;
}

/* add one verb to the given API */
const char* AfbAddOneVerb (AfbBinderHandleT *binder, afb_api_x4_t apiv4, json_object *configJ,
        afb_req_callback_x4_t callback, void *vcbData) {
    char *errorMsg=NULL;
    const char *uid=NULL, *verb=NULL, *info=NULL, *auth=NULL;
    const uint32_t session=0;
    int err, regex=0;
    const afb_auth *acl=NULL;

    /* scan the verb specification */
    err= rp_jsonc_unpack (configJ, "{s?s s?s s?s s?s s?i s?b}"
        , "uid"     , &uid
        , "verb"    , &verb
        , "info"    , &info
        , "auth"    , &auth
        , "session" , &session
        , "regex"   , &regex
        );
    if (err || (!verb && !uid)) {
        errorMsg = "config parsing error or missing both verb and uid fields";
        goto OnErrorExit;
    }

    // info verb require 'uid' but syntax allows to defined only one of uid|verb
    if (!verb && uid)  verb=uid;
    if (verb &&  !uid) json_object_object_add(configJ, "uid", json_object_new_string(verb));

    /* get permission description */
    if (auth) {
        acl= AfbAclSearch (binder->config.acls, auth);
        if (acl == NULL) {
            errorMsg = "'auth/acl' is undefined";
            goto OnErrorExit;
        }
    }

    /* create the verb */
    err= afb_api_v4_add_verb_hookable (apiv4, verb, info, callback, vcbData, acl, session, regex);
    if (err) {
        switch (err) {
        case X_EEXIST:
            errorMsg = "verb already exists/registered";
            break;
        case X_ENOMEM:
            errorMsg = "memory exhausted";
            break;
        case X_EPERM:
            errorMsg = "permission denied";
            break;
        default:
            errorMsg = strerror(-err);
            break;
        }
        goto OnErrorExit;
    }

    /* lock the config */
    json_object_get (configJ);
    return NULL;

OnErrorExit:
    json_object_object_add(configJ, "error", json_object_new_string(errorMsg));
    errorMsg= (char*)json_object_get_string(configJ);
    return errorMsg;
}

/**
 * @brief Structure for creaating verbs in callback
 */
typedef struct {

    /** the error message or NULL */
    const char *errorMsg;

    /** the binder */
    AfbBinderHandleT *binder;

    /** the API */
    afb_api_x4_t apiv4;

    /** the callback */
    afb_req_callback_x4_t callback;
}
    AddVerbsT;

/**
 * @brief Callback for creating verbs in AddVerbsCb
 *
 * @param context the AddVerbsT data
 * @param verbJ the config of the verb to add
 * @return 0 on success of a negative vaue on error
 */
static int AddVerbsCb(void *context, json_object *verbJ) {
    AddVerbsT *adder = (AddVerbsT*)context;
    AfbVcbDataT *vcbData = calloc (1, sizeof(AfbVcbDataT));

    if (vcbData == NULL)
        adder->errorMsg = "out of memory";
    else {
        vcbData->magic= (void*)AfbAddVerbs;
        vcbData->configJ= verbJ;
        vcbData->uid= json_object_get_string (json_object_object_get(verbJ, "uid"));
        adder->errorMsg= AfbAddOneVerb (adder->binder, adder->apiv4, verbJ, adder->callback, vcbData);
        if (adder->errorMsg) free(vcbData);
        else json_object_get(verbJ);
    }
    return adder->errorMsg == NULL ? 0 : -1;
}

/* add the verbs as described by JSON configJ */
const char* AfbAddVerbs (AfbBinderHandleT *binder, afb_api_x4_t apiv4, json_object *configJ, afb_req_callback_x4_t callback) {
    AddVerbsT adder;

    adder.errorMsg = NULL;
    adder.binder = binder;
    adder.apiv4 = apiv4;
    adder.callback = callback;
    rp_jsonc_optarray_until(configJ, AddVerbsCb, &adder);
    return adder.errorMsg;
}

/* add one event handler */
const char* AfbAddOneEvent (afb_api_x4_t apiv4, const char*uid, const char*pattern, afb_event_handler_x4_t callback, void *context) {
    int err = afb_api_v4_event_handler_add_hookable (apiv4, pattern, callback, context);
    return err < 0 ? "AfbAddOneEvent failed" : NULL;
}

/**
 * @brief Structure for creating events in callback
 */
typedef struct {

    /** the error message or NULL */
    const char *errorMsg;

    /** the API */
    afb_api_x4_t apiv4;

    /** the callback */
    afb_event_handler_x4_t callback;
}
    AddEventsT;

/**
 * @brief Callback for creating verbs in AddVerbsCb
 *
 * @param context the AddVerbsT data
 * @param verbJ the config of the verb to add
 * @return 0 on success of a negative vaue on error
 */
static int AddEventsCb(void *context, json_object *eventJ) {
    AddEventsT *adder = (AddEventsT*)context;
    AfbVcbDataT *vcbData = calloc (1, sizeof(AfbVcbDataT));
    int err;
    const char *uid = NULL, *pattern = NULL;

    if (vcbData == NULL)
        adder->errorMsg = "out of memory";
    else {
        err= rp_jsonc_unpack (eventJ, "{ss s?s}"
            , "uid", &uid
            , "pattern", &pattern
        );
        if (err)
            adder->errorMsg=json_object_get_string(eventJ);
        else {
            vcbData->magic= (void*)AfbAddEvents;
            vcbData->configJ= eventJ;
            vcbData->uid= uid;
            adder->errorMsg= AfbAddOneEvent (adder->apiv4, uid, pattern, adder->callback, vcbData);
        }
        if (adder->errorMsg) free(vcbData);
        else json_object_get(eventJ);
    }
    return adder->errorMsg == NULL ? 0 : -1;
}

/* add the verbs as described by JSON configJ */
const char* AfbAddEvents (afb_api_x4_t apiv4, json_object *configJ, afb_event_handler_x4_t callback) {
    AddEventsT adder;

    adder.errorMsg = NULL;
    adder.apiv4 = apiv4;
    adder.callback = callback;
    rp_jsonc_optarray_until(configJ, AddEventsCb, &adder);
    return adder.errorMsg;
}

/* delete on event of givven pattern */
const char* AfbDelOneEvent(afb_api_x4_t apiv4, const char*pattern, void **context) {
    int err = afb_api_v4_event_handler_del_hookable(apiv4, pattern, context);
    return err < 0 ? "AfbDelOneEvent failed" : NULL;
}


/**
 * @brief fill the config structure accordingly to the sonfiguration
 * json object
 *
 * @param configJ the JSON specification
 * @param config  the config structure
 * @return NULL on success or the text of error
 */
static const char *AfbApiConfig (json_object *configJ, AfbApiConfigT *config) {
    int err;
    const char*export=NULL;

    // allocate config and set defaults
    memcpy (config, &apiConfigDflt, sizeof(AfbApiConfigT));

    err= rp_jsonc_unpack (configJ, "{ss s?s s?s s?i s?s s?b s?o s?s s?s s?b s?o s?o s?s}"
        , "uid"    , &config->uid
        , "api"    , &config->api
        , "info"   , &config->info
        , "verbose", &config->verbose
        , "export" , &export
        , "noconcurrency", &config->noconcurency
        , "verbs"  , &config->verbsJ
        , "require", &config->require
        , "uri"    , &config->uri
        , "lazy"   , &config->lazy
        , "alias"  , &config->aliasJ
        , "events" , &config->eventsJ
        , "provide", &config->provide
        );
    if (err) return "invalid api configuration";

    // if api not defined use uid
    if (!config->api)  config->api= config->uid;

    if (export) {
        config->export= utilLabel2Value(afbApiExportKeys, export);
        if (config->export < 0) return "invalid export export key";
    }

    if (config->verbose) config->verbose= verbosity_to_mask(config->verbose);

    // if restricted provide a default URI
    if (config->export == AFB_EXPORT_RESTRICTED && !config->uri) {
        char uri[256];
        snprintf (uri, sizeof(uri), "unix:@%s", config->uid);
        config->uri= strdup (uri);
    }

    json_object_get(configJ);
    return NULL;
}

/**
 * @brief structure for initialisation of apis
 */
typedef struct {
    /** the binder structure */
    AfbBinderHandleT *binder;

    /** error message of the callback */
    const char* errorMsg;

    /** callback for the api */
    afb_api_callback_x4_t usrApiCb;

    /** callback for "info" requests */
    afb_req_callback_x4_t usrInfoCb;

    /** callback for requests */
    afb_req_callback_x4_t usrRqtCb;

    /** callback for events */
    afb_event_handler_x4_t usrEvtCb;

    /** declare set of the api */
    struct afb_apiset* apiDeclSet;

    /** call set of the api */
    struct afb_apiset* apiCallSet;

    /** userdata for the api */
    void *userData;

    /** the config object */
    AfbApiConfigT config;

} AfbApiInitT;

/**
 * @brief pre initialisation callback for API. Settingthe API requires that it is
 * created but creating the API is better achieved as a transactionnal operation
 * including its setup. This is why the "preinit" is done in a callback of
 * afb_api_v4_create.
 *
 * @param apiv4 the api currently created
 * @param context the context, here, the binder
 * @return return a positive or nul value on success or a negative value to
 *          return an error and cancel the API's creation
 */
static int AfbApiPreInit (afb_api_v4 *apiv4, void *context) {
    AfbApiInitT *init= (AfbApiInitT*) context;
    int status = 0;

    // set main data
    afb_api_v4_logmask_set (apiv4, init->config.verbose);
    afb_api_v4_set_userdata(apiv4, init->userData);
    afb_api_v4_set_mainctl(apiv4, init->usrApiCb);

    /* setup dependencies */
    if (init->config.provide != NULL)
        status =  afb_api_v4_class_provide(apiv4, init->config.provide);
    if (status == 0 && init->config.require != NULL)
        status =  afb_api_v4_class_require(apiv4, init->config.require);

    /* create info verb if required */
    if (init->usrInfoCb != NULL) {
        json_object *infoJ;
        rp_jsonc_pack (&infoJ, "{ss ss}"
            ,"verb", "info"
            ,"info", "AfbGlue implicit api introspection api verb"
        );

        init->errorMsg= AfbAddOneVerb (init->binder, apiv4, infoJ, init->usrInfoCb, init->userData);
        if (init->errorMsg) goto OnErrorExit;
    }

    /* setup event handler if required */
    if (init->config.eventsJ != NULL) {
        init->errorMsg= AfbAddEvents (apiv4, init->config.eventsJ, init->usrEvtCb);
        if (init->errorMsg) goto OnErrorExit;
    }

    /* declare the verbs now if required */
    if (init->config.verbsJ != NULL) {
        init->errorMsg= AfbAddVerbs (init->binder, apiv4, init->config.verbsJ, init->usrRqtCb);
        if (init->errorMsg) goto OnErrorExit;
    }

    /* seal the API if required */
    if (init->config.seal) afb_api_v4_seal_hookable (apiv4);

    // call preinit now
    status= afb_api_v4_safe_ctlproc (apiv4, init->usrApiCb, afb_ctlid_Pre_Init, (afb_ctlarg_t)NULL);
    return status;

OnErrorExit:
    return -1;
}

/* create an API Accordingly to what configJ describes */
const char* AfbApiCreate (AfbBinderHandleT *binder, json_object *configJ, afb_api_x4_t *apiv4,
    afb_api_callback_x4_t usrApiCb, afb_req_callback_x4_t usrInfoCb, afb_req_callback_x4_t usrRqtCb,
    afb_event_handler_x4_t usrEvtCb, void *userData) {
    int err, status;
    const char *errorMsg=NULL;;
    AfbApiInitT apiInit;

    /* check argument */
    if (binder == NULL) {
        errorMsg= "nonexistent binder instance";
        goto OnErrorExit;
    }

    /* extract config values */
    errorMsg= AfbApiConfig(configJ, &apiInit.config);
    if (errorMsg != NULL) goto OnErrorExit;

    // prepare context for preinit function
    apiInit.binder= binder;
    apiInit.usrApiCb= usrApiCb;
    apiInit.usrInfoCb=usrInfoCb;
    apiInit.usrEvtCb= usrEvtCb;
    apiInit.usrRqtCb= usrRqtCb;
    apiInit.userData= userData;

    /* compute declareset and callset */
    apiInit.apiCallSet   = binder->privateApis;
    switch (apiInit.config.export) {
        case AFB_EXPORT_PUBLIC:
            apiInit.apiDeclSet= binder->publicApis;
            break;

        case AFB_EXPORT_RESTRICTED:
            apiInit.apiDeclSet= binder->restrictedApis;
            break;

        case AFB_EXPORT_PRIVATE:
            apiInit.apiDeclSet= binder->privateApis;
            break;

        default:
            errorMsg="invalid api export value";
            goto OnErrorExit;
    }

    // register API
    status = afb_api_v4_create (apiv4, apiInit.apiDeclSet, apiInit.apiCallSet,
                                    apiInit.config.api, Afb_String_Const,
                                    apiInit.config.info, Afb_String_Const,
                                    apiInit.config.noconcurency,
                                    AfbApiPreInit, &apiInit, // pre-ctrlcb + ctx pre-init
                                    NULL, Afb_String_Const  // no binding.so path
    );
    if (status) {
        errorMsg= apiInit.errorMsg;
        goto OnErrorExit;
    }

    /* add HTTP aliases if needed */
    if (apiInit.config.aliasJ) {
        if (rp_jsonc_optarray_until(apiInit.config.aliasJ, BinderAddOneAlias, binder) < 0) {
            errorMsg= "afb_api registering aliases fail";
            goto OnErrorCleanAndExit;
        }
    }

    // if URI provided and api export allow then export it now
    if (apiInit.config.uri && apiInit.config.export != AFB_EXPORT_PRIVATE) {
        err= afb_api_ws_add_server(apiInit.config.uri, binder->restrictedApis, binder->restrictedApis);
        if (err) {
            errorMsg= "Fail to parse afbApi json config";
            goto OnErrorCleanAndExit;
        }
    }

    /* lock config */
    json_object_get (configJ);
    return NULL;

OnErrorCleanAndExit:
    afb_api_v4_unref(*apiv4);

OnErrorExit:
    *apiv4=NULL;
    return errorMsg;
}

/* import the API described by JSON configJ */
const char* AfbApiImport (AfbBinderHandleT *binder, json_object *configJ) {
    int err, index;
    const char *errorMsg=NULL;;
    afb_apiset *apiset;
    AfbApiConfigT config;

    /* check argument */
    if (binder == NULL) {
        errorMsg= "nonexistent binder instance";
        goto OnErrorExit;
    }

    /* extract configuration */
    errorMsg= AfbApiConfig(configJ, &config);
    if (errorMsg != NULL) goto OnErrorExit;

    /* deduce declare set from exportation */
    switch (config.export) {
        case AFB_EXPORT_PUBLIC:
            apiset= binder->publicApis;
            break;

        case AFB_EXPORT_RESTRICTED:
            apiset= binder->restrictedApis;
            break;

        case AFB_EXPORT_PRIVATE:
        default:
            apiset=binder->privateApis;
            break;
    }

    /* add the api */
    err = afb_api_ws_add_client (config.uri, apiset , binder->privateApis, !config.lazy);
    if (err) {
        errorMsg="Invalid imported api URI";
        goto OnErrorExit;
    }

    // Extract API from URI (TODO CHECK IF MEANINGFUL)
    for (index = (int)strlen(config.uri)-1; index > 0; index--) {
        if (config.uri[index] == '@' || config.uri[index] == '/') break;
    }

    return NULL;

  OnErrorExit:
    return errorMsg;
}

/**
* @brief configure and setup the HTTP server
*
* @param binder the binder instance
*
 * @return NULL on success or an error string
*/
static char* AfbBinderHttpd(AfbBinderHandleT *binder) {
    char *errorMsg=NULL;;
    int status;

    // the minimum we need is a listening port
    if (binder->config.httpd.port <= 0) {
        errorMsg= "invalid port";
        goto OnErrorExit;
    }

    // initialize cookie
    if (!afb_hreq_init_cookie(binder->config.httpd.port, 0, binder->config.httpd.timeout.session)) {
        errorMsg= "HTTP cookies init";
        goto OnErrorExit;
    }

    // set upload directory
    if (afb_hreq_init_download_path(binder->config.httpd.updir) < 0) {
        errorMsg= "set upload dir";
        goto OnErrorExit;
    }

    // create afbserver
    binder->hsrv = afb_hsrv_create();
    if (binder->hsrv == NULL) {
        errorMsg= "Allocating afb_hsrv_create";
        goto OnErrorExit;
    }

    // initialize the cache timeout
    if (!afb_hsrv_set_cache_timeout(binder->hsrv, binder->config.httpd.timeout.cache)) {
        errorMsg= "Allocating afb_hsrv_create";
        goto OnErrorExit;
    }

    // set the root api handlers for http websock
    if (!afb_hsrv_add_handler(binder->hsrv, binder->config.httpd.rootapi, afb_hswitch_websocket_switch, binder->publicApis, 20)) {
        errorMsg= "Allocating afb_hswitch_websocket_switch";
        goto OnErrorExit;
    }

    // set root api for http rest
    if (!afb_hsrv_add_handler(binder->hsrv, binder->config.httpd.rootapi, afb_hswitch_apis,binder->publicApis, 10)) {
        errorMsg= "Allocating afb_hswitch_apis";
        goto OnErrorExit;
    }

    // set OnePageApp rootdir
    if (!afb_hsrv_add_handler(binder->hsrv,  binder->config.httpd.onepage, afb_hswitch_one_page_api_redirect, NULL, -20)) {
        errorMsg= "Allocating one_page_api_redirect";
        goto OnErrorExit;
    }

    // loop to register all aliases

    if (binder->config.httpd.aliasJ) {
        if (rp_jsonc_optarray_until (binder->config.httpd.aliasJ, BinderAddOneAlias, binder) < 0) {
            errorMsg= "Registering aliases";
            goto OnErrorExit;
        }
    }

    // set server rootdir path
    status= afb_hsrv_add_alias(binder->hsrv, "", afb_common_rootdir_get_fd(), binder->config.httpd.basedir, -10, 1);
    if (status != AFB_HSRV_OK) {
        errorMsg= "Registering httpd basedir";
        goto OnErrorExit;
    }

    return NULL;

OnErrorExit:
    if (binder->hsrv) afb_hsrv_put(binder->hsrv);
    LIBAFB_ERROR("AfbBinderHttpd: fail to start httpd server [%s]", errorMsg);
    return errorMsg;
}

/* get some binder config data */
const char* AfbBinderInfo (AfbBinderHandleT *binder, const char*key) {
    const char* value;
    char number[32];
    AfbBinderInfoE rqt= utilLabel2Value (afbBinderInfoKeys, key);
    switch (rqt) {

        case BINDER_INFO_UID:
            value= strdup (binder->config.uid);
            break;

        case BINDER_INFO_PORT:
            snprintf (number, sizeof(number),"%i", binder->config.httpd.port);
            value= strdup (number);
            break;

        case BINDER_INFO_ROOTDIR:
            value= strdup (binder->config.rootdir);
            break;

        case BINDER_INFO_HTTPDIR:
            value= strdup (binder->config.httpd.basedir);
            break;

        default: goto OnErrorExit;
    }

    return value;

OnErrorExit:
    return NULL;
}

/**
 * @brief structure for searching paths
 */
typedef struct {
    /** the base filename to search */
    const char *filename;
    /** the built path searched for */
    char path[PATH_MAX + 1];
} ScanPathT;

/**
 * @brief search for file within dirnameJ returns an open file decriptor on file when exist
 *
 * @param context the search structure
 * @param dirnameJ the JSON path to check
 * @return 0 to continue searching or 1 if match found
 */
static int ScanPathCb (void *context, json_object *dirnameJ) {
    ScanPathT *scanner = (ScanPathT*)context;
    const char* dirname= json_object_get_string(dirnameJ);
    snprintf(scanner->path, sizeof scanner->path, "%s/%s", dirname, scanner->filename);
    return 0 == access(scanner->path, R_OK);
}

/* load a binding as described by bindingJ */
const char* AfbBindingLoad (AfbBinderHandleT *binder, json_object *bindingJ) {
    const char *errorMsg=NULL, *uri=NULL;
    afb_apiset *apiDeclSet, *apiCallSet;
    int err;
    const char *uid=NULL, *libpath, *export=NULL;
    json_object *aliasJ=NULL, *ldpathJ=NULL, *configJ=NULL;
    ScanPathT scanner;

    /* check argument */
    if (binder == NULL) {
        errorMsg= "nonexistent binder instance";
        goto OnErrorExit;
    }

    /* extract api specification */
    err= rp_jsonc_unpack (bindingJ, "{ss ss s?s s?s s?o s?o}"
        ,"uid"    , &uid
        ,"path"   , &libpath
        ,"export" , &export
        ,"uri"    , &uri
        ,"ldpath" , &ldpathJ
        ,"alias"  , &aliasJ
    );
    if (err) {
        errorMsg= "fail parsing json binding config";
        goto OnErrorExit;
    }

    /* compute declare set and call set */
    apiCallSet= binder->privateApis;
    int exportMod= utilLabel2Value(afbApiExportKeys, export);
    switch (exportMod) {
        case AFB_EXPORT_PUBLIC:
            apiDeclSet= binder->publicApis;
            break;

        case AFB_EXPORT_RESTRICTED:
            apiDeclSet= binder->restrictedApis;
            if (!uri) {
                char buffer[256];
                snprintf (buffer, sizeof(buffer), "unix:@%s", uid);
                uri=strdup(buffer);
            }
            break;

        case AFB_EXPORT_PRIVATE:
            apiDeclSet= binder->privateApis;
            break;

        default:
            errorMsg="invalid api export value";
            goto OnErrorExit;
    }

    /* add aliases */
    if (aliasJ && binder->config.httpd.port) {
        if (rp_jsonc_optarray_until (aliasJ, BinderAddOneAlias, binder) < 0) {
            errorMsg= "afb_api registering aliases fail";
            goto OnErrorExit;
        }
    }

    // try to locate the binding within binding/binder ldpath
    scanner.filename = libpath;
    if (libpath[0] != '/') {
        err = ldpathJ == NULL ? 0 : rp_jsonc_optarray_until (ldpathJ, ScanPathCb, &scanner);
        if (err == 0 && binder->config.ldpathJ != NULL)
            err = rp_jsonc_optarray_until (binder->config.ldpathJ, ScanPathCb, (void*)libpath);
        if (err != 0) scanner.filename = scanner.path;
    }

    // open the binding
    err= afb_api_so_add_binding_config(scanner.filename, apiDeclSet, apiCallSet, configJ);
    if (err) {
        LIBAFB_ERROR ("AfbBindingLoad:fatal [uid=%s] can't open %s", uid, libpath);
        errorMsg= "binding load fail";
        goto OnErrorExit;
    }

    /* export the binding */
    if (uri && exportMod != AFB_EXPORT_PRIVATE) {
        err= afb_api_ws_add_server(uri, apiDeclSet, apiDeclSet);
        if (err) {
            errorMsg= "exportation failed";
            goto OnErrorExit;
        }
    }

    return NULL;

OnErrorExit:
    LIBAFB_ERROR ("AfbBindingLoad:fatal [uid=%s] %s", uid == NULL ? "?" : uid, errorMsg);
    return errorMsg;
}

// binder API control callback
static int AfbBinderCtrlCb(afb_api_x4_t apiv4, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg, void *context) {
    static int count=0;
    switch (ctlid) {
        case afb_ctlid_Orphan_Event:
            /* Handle orphan event */
            afb_api_v4_verbose (apiv4, AFB_SYSLOG_LEVEL_INFO, __file__,__LINE__,__func__,
                                 "orphan event=[%s] count=[%d]", ctlarg->orphan_event.name, count++);
            break;
        //case afb_ctlid_Root_Entry:
        //case afb_ctlid_Pre_Init:
        //case afb_ctlid_Init:
        //case afb_ctlid_Class_Ready:
        //case afb_ctlid_Exiting:
        default:
            break;
    }
    return 0;
}

/* parse JSON configuration of binder */
static int BinderParseConfig (json_object *configJ, AfbBinderConfigT *config) {
    int err;
    json_object *ignoredJ;
    json_object *aclsJ=NULL;

    // allocate config and set defaults
    memcpy (config, &binderConfigDflt, sizeof(AfbBinderConfigT));

    err= rp_jsonc_unpack (configJ, "{ss s?s s?i s?i s?b s?i s?s s?s s?s s?s s?s s?o s?o s?o s?o s?o s?i s?i s?o !}"
        , "uid"    , &config->uid
        , "info"   , &config->info
        , "verbose", &config->verbose
        , "timeout", &config->timeout
        , "noconcurrency", config->noconcurency
        , "port", &config->httpd.port
        , "roothttp", &config->httpd.basedir
        , "rootapi", &config->httpd.rootapi
        , "rootdir", &config->rootdir
        , "https-cert", &config->httpd.cert
        , "https-key" , &config->httpd.key
        , "alias", &config->httpd.aliasJ
        , "intf", &config->httpd.intfJ
        , "extentions", &config->extendJ
        , "ldpath", &config->ldpathJ
        , "acls", &aclsJ
        , "thread-pool", &config->poolThreadSize
        , "thread-max" , &config->poolThreadMax
        , "onerror", &ignoredJ
        );
    if (err) goto OnErrorExit;

    // move from level to mask
    if (config->verbose) config->verbose= verbosity_to_mask(config->verbose);

    /* get credentials */
    if (aclsJ) {
        config->acls= AfbAclBuildFromJsonC (aclsJ);
        if (!config->acls) goto OnErrorExit;
    }

    /* check thread settings */
    if (config->poolThreadMax <= 0)
        config->poolThreadMax = 1;
    if (config->poolThreadMax < config->poolThreadSize)
        config->poolThreadSize = config->poolThreadMax;
    return 0;

OnErrorExit:
    return -1;
}

/* Create a binder context for the given config */
const char* AfbBinderConfig (json_object *configJ, AfbBinderHandleT **handle, void *userdata) {
    const char *errorMsg=NULL;;
    AfbBinderHandleT *binder=NULL;
    int status;
    unsigned traceFlags;

    // create binder handle and select default config
    binder= calloc (1, sizeof(AfbBinderHandleT));
    if (binder == NULL) {
        errorMsg= "can't allocate binder data structures";
        goto OnErrorExit;
    }

    /* parse the config */
    status= BinderParseConfig (configJ, &binder->config);
    if (status < 0) {
        errorMsg= "failed to parse binder configuration";
        goto OnErrorExit;
    }

    // set binder global verbosity
    afb_verbose_set(verbosity_to_mask(binder->config.verbose));

    // create the apisets: private, protected and public
    binder->privateApis = afb_apiset_create("restricted", binder->config.timeout);
    if (!binder->privateApis) {
        errorMsg= "can't create main apiset";
        goto OnErrorExit;
    }
    binder->restrictedApis = afb_apiset_create_subset_first(binder->privateApis, "restricted", binder->config.timeout);
    if (!binder->restrictedApis) {
        errorMsg= "can't create restricted apiset";
        goto OnErrorExit;
    }
    binder->publicApis = afb_apiset_create_subset_first(binder->restrictedApis, "public", binder->config.timeout);
    if (!binder->publicApis) {
        errorMsg= "can't create public apiset";
        goto OnErrorExit;
    }

    // setup global private api to handle events
    status = afb_api_v4_create (&binder->apiv4, binder->privateApis, binder->privateApis,
                                binder->config.uid, Afb_String_Const,
                                NULL, Afb_String_Const,
                                1, // no concurency
                                NULL, NULL, // pre-ctrlcb + ctx pre-init
                                NULL, Afb_String_Const  // no binding.so path
    );
    if (status < 0) {
        errorMsg= "failed to create internal private binder API";
        goto OnErrorExit;
    }
    afb_api_set_userdata(binder->apiv4, userdata);
    afb_api_v4_set_mainctl(binder->apiv4, AfbBinderCtrlCb);

    /* init global API */
    afb_global_api_init(binder->privateApis);

    /* setup the monitoring */
    status = afb_monitor_init(binder->publicApis, binder->restrictedApis);
    if (status < 0) {
        errorMsg= "failed to setup monitor";
        goto OnErrorExit;
    }

    /* setup the supervision */
    if (binder->config.supervision) {
        status = afb_supervision_init(binder->privateApis, configJ);
        if (status < 0) {
            errorMsg= "failed to setup supervision";
            goto OnErrorExit;
        }
    }

    /* steup tracing of request, api, event, session or global events */
    if (binder->config.trace.rqt) {
        status = afb_hook_flags_req_from_text(binder->config.trace.rqt, &traceFlags);
        if (status < 0) {
            errorMsg= "invalid tracereq";
            goto OnErrorExit;
        }
        afb_hook_create_req(NULL, NULL, NULL, traceFlags, NULL, NULL);
    }
    if (binder->config.trace.api) {
        status = afb_hook_flags_api_from_text(binder->config.trace.api, &traceFlags);
        if (status < 0) {
            errorMsg= "invalid traceapi";
            goto OnErrorExit;
        }
        afb_hook_create_api(NULL, traceFlags, NULL, NULL);
    }
    if (binder->config.trace.evt) {
        status = afb_hook_flags_evt_from_text(binder->config.trace.evt, &traceFlags);
        if (status < 0) {
            errorMsg= "invalid traceevt";
            goto OnErrorExit;
        }
        afb_hook_create_evt(NULL, traceFlags, NULL, NULL);
    }
    if (binder->config.trace.ses) {
        status = afb_hook_flags_session_from_text(binder->config.trace.ses, &traceFlags);
        if (status < 0) {
            errorMsg= "invalid traceses";
            goto OnErrorExit;
        }
        afb_hook_create_session(NULL, traceFlags, NULL, NULL);
    }
    if (binder->config.trace.glob) {
        status = afb_hook_flags_global_from_text(binder->config.trace.glob, &traceFlags);
        if (status < 0) {
            errorMsg= "invalid traceglob";
            goto OnErrorExit;
        }
        afb_hook_create_global(traceFlags, NULL, NULL);
    }

    /* load the extensions if existing */
    if (binder->config.extendJ) {
        status = afb_extend_config(binder->config.extendJ);
        if (status < 0) {
            errorMsg= "Extension config failed";
            goto OnErrorExit;
        }
    }

    /* set the root directory */
    if (afb_common_rootdir_set(binder->config.rootdir) < 0) {
            errorMsg= "Rootdir set fail";
            goto OnErrorExit;
    }

    /* start HTTP service */
    if (binder->config.httpd.port) {
        errorMsg = AfbBinderHttpd(binder);
        if (errorMsg) goto OnErrorExit;
    }

    /* done, no error */
    json_object_get(configJ);
    *handle= binder;
    return NULL;

OnErrorExit:
    LIBAFB_ERROR ("%s:fatal %s", __func__, errorMsg);
    if (binder) free (binder);
    *handle=NULL;
    return errorMsg;
}

/* returns the log mask */
int AfbBinderGetLogMask(AfbBinderHandleT *binder) {
    return afb_verbose_get();
}

/* get the global API */
afb_api_x4_t AfbBinderGetApi (AfbBinderHandleT *binder) {
    return binder->apiv4;
}

/**
 * @brief Callback for creating HTTP interfaces
 *
 * @param context the binder context
 * @param intfJ the interface or NULL for setting the port
 * @return 0 on success of a negative vaue on error
 */
static int BinderAddIntf(void* context, json_object *intfJ) {
    AfbBinderHandleT *binder = (AfbBinderHandleT*)context;
    int status;
    char buffer[512];
    const char* intf;

    if (intfJ != NULL)
        /* use string as interface */
        intf = json_object_get_string (intfJ);
    else {
        /* make the interface using the port */
        status = snprintf(buffer, sizeof(buffer), "tcp:%s:%d",
                    DEFAULT_BINDER_INTERFACE, binder->config.httpd.port);
        if (status >= (int)sizeof(buffer)) goto OnErrorExit;
        intf = buffer;
    }

    /* create the interfavce */
    status= afb_hsrv_add_interface(binder->hsrv, intf);
    if (status >= 0)
        return 0;

OnErrorExit:
    LIBAFB_ERROR ("BinderAddIntf: fail adding interface=%s", intf);
    return -1;
}

/**
* @brief structure for starting 
*/
typedef struct {
    /** the binder instance */
    AfbBinderHandleT *binder;

    /** the callback for starting */
    AfbStartupCb callback;

    /** config argument for the callback */
    void *config;

    /** context argument for the callback */
    void *context;
}
    AfbBinderInitT;

/**
* @brief callback for starting the binder: it starts
* the services (apis), the http server and calls the
* given value.
*/
void BinderStartCb (int signum, void *context) {
    const char *errorMsg=NULL;;
    AfbBinderInitT *init = (AfbBinderInitT*)context;
    AfbBinderHandleT *binder= init->binder;
    int status = 0;

    /* process exceptional case */
    if (signum > 0) {
        errorMsg= "signal catched in start";
        goto OnErrorExit;
    }

    // resolve dependencies and start binding services
    status= afb_apiset_start_all_services (binder->privateApis);
    if (status) {
        errorMsg= "failed to start all services";
        goto OnErrorExit;
    }

    /* start HTTP server and its interfaces */
    if (binder->config.httpd.port) {
        status= !afb_hsrv_start_tls(binder->hsrv, 15, binder->config.httpd.cert, binder->config.httpd.key);
        if (status) {
            errorMsg= "failed to start binder httpd service";
            goto OnErrorExit;
        }

        // start interface
        status = rp_jsonc_optarray_until (binder->config.httpd.intfJ, BinderAddIntf, binder);
        if (status) {
            errorMsg="failed to add interface to the binder";
            goto OnErrorExit;
        }
    }
    // start user startup function
    if (init->callback) {
        status= init->callback (init->config, init->context);
        if (status < 0) {
            errorMsg= "binder startup callback errored out";
            goto OnErrorExit;
        }
    }

    // if status == 0 keep mainloop running
    if (!status)
        LIBAFB_NOTICE ("Binder [%s] running", binder->config.uid);
    else if (signum >= 0)
        AfbBinderExit(binder, status);
    return;

OnErrorExit:
    // errorMsg should have been set if we end up here
    LIBAFB_WARNING ("%s: failure: error message=[%s]", __func__, errorMsg);
    if (signum >= 0) AfbBinderExit(binder, status);
}

// start binder scheduler within a new thread
int AfbBinderStart (AfbBinderHandleT *binder, void *config, AfbStartupCb callback, void *context) {
    AfbBinderInitT binderCtx;
    binderCtx.config= config;
    binderCtx.binder= binder;
    binderCtx.context=context;
    binderCtx.callback= callback;

    int status= afb_sched_start(
                    binder->config.poolThreadMax,
                    binder->config.poolThreadSize,
                    binder->config.maxJobs,
                    BinderStartCb,
                    &binderCtx);
    return status;
}

// start binder scheduler within current thread context
int AfbBinderEnter (AfbBinderHandleT *binder, void *config, AfbStartupCb callback, void *context) {
    AfbBinderInitT binderCtx;
    binderCtx.config= config;
    binderCtx.binder= binder;
    binderCtx.context=context;
    binderCtx.callback= callback;

    BinderStartCb(-1, &binderCtx);
    return 0;
}

// Force mainloop exit
void AfbBinderExit(AfbBinderHandleT *binder, int exitcode) {
    afb_sched_exit (1, NULL /*callback*/, NULL/*context*/, exitcode);
}

// pop afb waiting event and process them
void AfbPollRunJobs(void) {
    afb_ev_mgr_prepare();
    afb_ev_mgr_wait_and_dispatch(0);
    for (struct afb_job *job= afb_jobs_dequeue(0); job; job= afb_jobs_dequeue(0)) {
        afb_jobs_run(job);
    }
}
