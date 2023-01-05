/*
 * Copyright (C) 2015-2023 IoT.bzh Company
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

#pragma once

#include <json-c/json.h>
#include <libafb/afb-v4.h>

/**
 * @brief Magic tags for distinguishing object's instances
 */
typedef enum {
    AFB_UNKNOWN_MAGIC_TAG=0, /**< Default unset object identity */
    AFB_BINDER_MAGIC_TAG,    /**< Identify BINDER objects */
    AFB_API_MAGIC_TAG,       /**< Identify API objects */
    AFB_RQT_MAGIC_TAG,       /**< Identify REQUEST objects */
    AFB_EVT_MAGIC_TAG,       /**< Identify EVENT objects */
    AFB_TIMER_MAGIC_TAG,     /**< Identify TIMER objects */
    AFB_JOB_MAGIC_TAG,       /**< Identify JOB objects */
    AFB_POST_MAGIC_TAG,      /**< Identify POSTED JOB objects */
    AFB_CALL_MAGIC_TAG,      /**< Identify ASYNCHRONOUS CALL objects */
}
    AfbMagicTagE;

/**
 * @brief ?
 */
typedef struct {
    void* magic;
    json_object *configJ;
    const char *uid;
    void *callback;
    void *thread;
    void *userdata;
    void *state;
} AfbVcbDataT;

/**
 * @brief opaque handler for binder structure
 */
typedef struct AfbBinderHandleS AfbBinderHandleT;

/**
 * @brief startup function
 * @see AfbBinderStart
 * @see AfbBinderEnter
 */
typedef int (*AfbStartupCb) (void *config, void *context);

/**
 * @brief Returns a string for the given magic tag value
 *
 * @param magic the tag value
 *
 * @return the text representing the magic tag or NULL
 * when the tag is not valid
 */
extern const char * AfbMagicToString (AfbMagicTagE magic);

/**
 * @brief Create a binder context for the given config
 *
 * @param configJ  the configuration object describing the binder to create
 * @param handle   receives the hendle to the created binder
 * @param userdata user to atach to the created binder
 *
 * @return NULL on success or an error string
 */
extern const char* AfbBinderConfig(json_object *configJ, AfbBinderHandleT **handle, void* userdata);

/**
 * @brief Load a binding library accordingly to JSON config bindingJ
 *
 * @param binder   the binder context
 * @param bindingJ JSON specification of the binding to load
 *
 * @return NULL on success or an error string
 */
extern const char* AfbBindingLoad(AfbBinderHandleT *binder, json_object *bindingJ);

/**
 * @brief Import an external API accordingly to config bindingJ
 *
 * @param binder   the binder context
 * @param configJ  JSON specification of the API to import
 *
 * @return NULL on success or an error string
 */
extern const char* AfbApiImport(AfbBinderHandleT *binder, json_object *configJ);

/**
 * @brief Creates an instance of API accordingly to JSON config configJ
 *
 * @param binder   the binder context
 * @param configJ  JSON specification of the API to create
 * @param afbAPI   pointer receiving the handler to the created API
 * @param usrApiCb callback of the API
 * @param usrInfoCb callback for INFO request (can be NULL)
 * @param usrRqtCb callback for handling requests
 * @param usrEvtCb callback for handling events
 * @param userdata user data for callbacks
 *
 * @return NULL on success or an error string
 */
extern const char* AfbApiCreate(
                    AfbBinderHandleT *binder,
                    json_object *configJ,
                    afb_api_x4_t *afbApi,
                    afb_api_callback_x4_t usrApiCb,
                    afb_req_callback_x4_t usrInfoCb,
                    afb_req_callback_x4_t usrRqtCb,
                    afb_event_handler_x4_t usrEvtCb,
                    void *userData);

/**
 * @brief Add one verb to the API of the binder accordingly to JSON configJ
 *
 * @param binder binder handler
 * @param apiv4 api handler
 * @param configJ specification of the verb to create
 * @param callback callback for handling the created verb
 * @param vcbData data to attach to the verb
 *
 * @return NULL on success or an error string
 */
extern const char* AfbAddOneVerb(AfbBinderHandleT *binder, afb_api_x4_t apiv4,
    json_object *configJ, afb_req_callback_t callback, void *vcbData);

/**
 * @brief Add to the API of the binder the verbs accordingly to JSON config configJ
 *
 * @param binder binder handler
 * @param apiv4 api handler
 * @param configJ specification of verbs to create
 * @param callback callback for handling created verbs
 *
 * @return NULL on success or an error string
 */
extern const char* AfbAddVerbs(AfbBinderHandleT *binder, afb_api_x4_t apiv4, json_object *configJ, afb_req_callback_t callback);

/**
 * @brief add one event handler for the api
 *
 * @param apiv4 the api
 * @param uid  unused
 * @param pattern the global pattern of the events to handle
 * @param callback the callback function that handle the events of pattern
 * @param context the closure callback data
 *
 * @return NULL on success or an error string
 */
extern const char* AfbAddOneEvent(afb_api_x4_t apiv4, const char*uid, const char*pattern, afb_event_handler_x4_t callback, void *context);

/**
 * @brief add event handlers for the api accordingly to JSON configJ
 *
 * @param apiv4 the api
 * @param configJ specification of events to create
 * @param callback the callback function that handle the events of pattern
 *
 * @return NULL on success or an error string
 */
extern const char* AfbAddEvents(afb_api_x4_t apiv4, json_object *configJ, afb_event_handler_t callback);

/**
 * @brief delete one event handler for the api
 *
 * @param apiv4 the api
 * @param pattern the global pattern of the handler to remove
 * @param context a pointer for storing the closure of the deleted handler
 *
 * @return NULL on success or an error string
 */
extern const char* AfbDelOneEvent(afb_api_x4_t apiv4, const char*pattern, void **context);

/**
 * @brief get the current global API
 *
 * This function is here for compatibility. Use 'afb_verbose_get'.
 *
 * @param binder the binder handler
 *
 * @return the global API
 */
extern afb_api_x4_t AfbBinderGetApi(AfbBinderHandleT *binder);

/**
 * @brief get the configuration value attached on a key
 *
 * Valid keys are:
 *    - "uid": the uid of the binder
 *    - "port": the port of the binder
 *    - "rootdir": the root directory
 *    - "httpdir": the served http directory

 * @param binder the binder handler
 * @param key the name of the configuration value to retrieve
 *
 * @return NULL on error or a copy of the value for the key
 *         the returned value must be freed using free
 */
extern const char* AfbBinderInfo(AfbBinderHandleT *binder, const char*key);

/**
 * @brief start the services of the binder and run its 
 *        scheduler until AfbBinderExit is called
 *
 * @param binder the binder handler
 * @param config the config for the callback
 * @param callback the callback function to call when scheduler has started (or NULL)
 * @param context the context for the callback
 *
 * @return an negative error code or the result calling the callback if it
 *        returned a not zero value or the result of the call to AfbBinderExit
 */ 
extern int AfbBinderStart(AfbBinderHandleT *binder, void *config, AfbStartupCb callback, void *context);

/**
 * @brief stop the binder's scheduler and return a status
 *
 * @param binder the binder handler
 * @param exitcode the status to return
 */
extern void AfbBinderExit(AfbBinderHandleT *binder, int exitcode);

/**
 * @brief start the services of the binder but not the scheduler
 *
 * @param binder the binder handler
 * @param config the config for the callback
 * @param callback the callback function to call when services have started (or NULL)
 * @param context the context for the callback
 *
 * @return an negative error code or the result calling the callback or zero
 *        if callback == NULL
 */ 
extern int AfbBinderEnter(AfbBinderHandleT *binder, void *config, AfbStartupCb callback, void *context);

/**
 * @brief Wait for an event, dispatch scheduled jobs available and returns
 */ 
extern void AfbPollRunJobs(void);

/*************************************************************************************/
/*************************************************************************************/
/***** D E P R E C A T E D                                                       *****/
/*************************************************************************************/
/*************************************************************************************/

/**
 * @brief get the current log mask
 *
 * This function is here for compatibility. Use 'afb_verbose_get'.
 *
 * @param binder unused
 *
 * @return the log mask
 */
extern int AfbBinderGetLogMask(AfbBinderHandleT *binder);

