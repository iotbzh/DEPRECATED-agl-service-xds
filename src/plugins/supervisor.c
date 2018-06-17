/*
 * Copyright (C) 2018 "IoT.bzh"
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ctl-plugin.h"
#include "supervisor-api.h"
#include "supervisor.h"
#include "wrap-json.h"

struct afb_cred {
    int refcount;
    uid_t uid;
    gid_t gid;
    pid_t pid;
    const char* user;
    const char* label;
    const char* id;
};

static const char* null_str = "null";

struct decode_daemon_str {
    DAEMONS_T* daemons;
    AFB_ApiT api;
    const char* ignored_daemon;
    int* ret_code;
};

static void decode_daemons_cb(void* closure, json_object* obj, const char* fName)
{
    int rc;
    struct afb_cred cred;
    json_object *j_response, *j_query, *j_config, *j_ws_servers, *j_ws_clients;
    json_object *j_name, *j_apis;
    struct decode_daemon_str* clStr = (struct decode_daemon_str*)closure;
    DAEMON_T* daemon = calloc(sizeof(DAEMON_T), 1);

    if (!clStr->daemons)
        return;

    if ((rc = wrap_json_unpack(obj, "{si si si ss ss ss}", "pid", &cred.pid,
             "uid", &cred.uid, "gid", &cred.gid, "id", &cred.id,
             "label", &cred.label, "user", &cred.user))
        < 0) {
        // TODO
        return;
    }

    AFB_ApiInfo(clStr->api, "Get supervisor/config - pid %d", cred.pid);
    daemon->pid = cred.pid;

    // Get config
    wrap_json_pack(&j_query, "{s:i}", "pid", cred.pid);
    rc = AFB_ServiceSync(clStr->api, SRV_SUPERVISOR_NAME, "config", j_query, &j_response);
    if (rc < 0) {
        AFB_ApiError(clStr->api, "Cannot get config of pid %d", cred.pid);
        *clStr->ret_code = rc;
        return;
    }

    AFB_ApiDebug(clStr->api, "%s config result, res=%s", SRV_SUPERVISOR_NAME,
        json_object_to_json_string(j_response));

    if (json_object_object_get_ex(j_response, "response", &j_config)) {
        // FIXME : implement free
        daemon->config = j_config;

        rc = wrap_json_unpack(j_config, "{s:o s:o s:o}",
            "name", &j_name,
            "ws_servers", &j_ws_servers,
            "ws_clients", &j_ws_clients);
        if (rc < 0) {
            AFB_ApiError(clStr->api, "Error decoding config response %s", wrap_json_get_error_string(rc));
            return;
        }

        daemon->name = json_object_is_type(j_name, json_type_null) ? null_str : json_object_get_string(j_name);

        // ignored some daemon
        if (clStr->ignored_daemon != NULL && strstr(daemon->name, clStr->ignored_daemon) != NULL) {
            free(daemon);
            return;
        }

        daemon->ws_servers = j_ws_servers;
        daemon->isServer = (json_object_array_length(j_ws_servers) > 0);
        daemon->ws_clients = j_ws_clients;
        daemon->isClient = (json_object_array_length(j_ws_clients) > 0);
    }

    // Get apis
    AFB_ApiInfo(clStr->api, "Get supervisor/do monitor get apis - pid %d", cred.pid);

    // '{"pid":6262,"api":"monitor","verb":"get","args":{"apis":true}}
    wrap_json_pack(&j_query, "{si ss ss s {sb}}",
        "pid", cred.pid,
        "api", "monitor",
        "verb", "get",
        "args", "apis", true);
    rc = AFB_ServiceSync(clStr->api, SRV_SUPERVISOR_NAME, "do", j_query, &j_response);
    if (rc < 0) {
        AFB_ApiError(clStr->api, "Cannot get apis of pid %d", cred.pid);
        return;
    } else {
        AFB_ApiDebug(clStr->api, "%s do ...get apis result, res=%s", SRV_SUPERVISOR_NAME, json_object_to_json_string(j_response));

        if (json_object_object_get_ex(j_response, "response", &j_config) && json_object_object_get_ex(j_config, "apis", &j_apis)) {
            // Don't forward monitor config details
            json_object_object_del(j_apis, "monitor");

            // Only forward apis verb without all details
            /* TODO
            j_newApis = json_object_new_object();
            json_object_object_foreach(json_object_object_get(j_apis, "apis"), key, val) {
                ...
            }
            */
            daemon->apis = j_apis;
        }
    }
    clStr->daemons->daemons[clStr->daemons->count] = daemon;
    clStr->daemons->count++;
}

int getDaemons(AFB_ApiT apiHandle, DAEMONS_T** daemons)
{
    int rc;
    json_object *j_response, *j_daemons = NULL;

    *daemons = calloc(sizeof(DAEMONS_T), 1);

    AFB_ApiInfo(apiHandle, "Call supervisor/discover");
    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "discover", NULL,
             &j_response))
        < 0) {
        return rc;
    }

    AFB_ApiInfo(apiHandle, "Call supervisor/list");
    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "list", NULL,
             &j_response))
        < 0) {
        return rc;
    }

    AFB_ApiInfo(apiHandle, "Get details info for each daemon");
    AFB_ApiDebug(apiHandle, "%s list result, res=%s", SRV_SUPERVISOR_NAME, json_object_to_json_string(j_response));

    if (!json_object_object_get_ex(j_response, "response", &j_daemons)) {
    }
    struct decode_daemon_str cls = {
        *daemons,
        apiHandle,
        apiHandle->apiname,
        &rc
    };
    wrap_json_object_for_all(j_daemons, decode_daemons_cb, &cls);

    return rc;
}

#define XDS_TAG_REQUEST "xds:trace/request"
#define XDS_TAG_EVENT "xds:trace/event"
#define XDS_TRACE_NAME "xds-trace"

int trace_daemon(AFB_ApiT apiHandle, DAEMON_T* dm, const char* level)
{
    int rc;
    json_object *j_response, *j_query, *j_tracereq, *j_traceevt;

    if (dm == NULL) {
        return -1;
    }

    // First drop previous traces
    // Note: ignored error (expected 1st time/when no trace exist)
    trace_drop(apiHandle, dm->pid);

    j_tracereq = json_object_new_array();
    if (level && !strncmp(level, "all", 3)) {
        json_object_array_add(j_tracereq, json_object_new_string("all"));
    } else {
        json_object_array_add(j_tracereq, json_object_new_string("life"));
        json_object_array_add(j_tracereq, json_object_new_string("reply"));
    }

    j_traceevt = json_object_new_array();
    if (level && !strncmp(level, "all", 3)) {
        json_object_array_add(j_traceevt, json_object_new_string("all"));
    } else {
        json_object_array_add(j_traceevt, json_object_new_string("common"));
    }

    // Configure tracing of specified daemon
    // request: monitor/trace({ "add": { "tag": "xds:trace/request", "name": "trace", "request": "all" } })
    wrap_json_pack(&j_query, "{s:i, s: [{s:s s:s s:o}, {s:s s:s s:o}] }",
        "pid", dm->pid, "add",
        "tag", XDS_TAG_REQUEST, "name", XDS_TRACE_NAME, "request", j_tracereq,
        "tag", XDS_TAG_EVENT, "name", XDS_TRACE_NAME, "event", j_traceevt);

    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "trace", j_query, &j_response)) < 0) {
        AFB_ApiError(apiHandle, "ERROR tracing pid %d result: %s", dm->pid,
            json_object_to_json_string(j_response));
        return rc;
    }

    return 0;
}

// FIXME prototype must be int trace_drop(AFB_ApiT apiHandle, DAEMON_T* dm)
int trace_drop(AFB_ApiT apiHandle, int pid)
{
    json_object *j_response, *j_query;

    // monitor/trace({ "drop": { "tag": "trace/request" } })
    wrap_json_pack(&j_query, "{s:i s:{s:[s s]}}", "pid", pid, "drop", "tag", XDS_TAG_REQUEST, XDS_TAG_EVENT);
    return AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "trace", j_query, &j_response);
}

int supervisor_init(void)
{
    return 0;
}
