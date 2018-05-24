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

    AFB_ApiInfo(clStr->api, "Get config of pid %d", cred.pid);
    daemon->pid = cred.pid;

    // Get config
    wrap_json_pack(&j_query, "{s:i}", "pid", cred.pid);
    rc = AFB_ServiceSync(clStr->api, SRV_SUPERVISOR_NAME, "config", j_query, &j_response);
    if (rc < 0) {
        AFB_ApiError(clStr->api, "Cannot get config of pid %d", cred.pid);
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

    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "discover", NULL,
             &j_response))
        < 0) {
        return rc;
    }

    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "list", NULL,
             &j_response))
        < 0) {
        return rc;
    }

    AFB_ApiDebug(apiHandle, "%s list result, res=%s", SRV_SUPERVISOR_NAME, json_object_to_json_string(j_response));

    if (json_object_object_get_ex(j_response, "response", &j_daemons)) {
        struct decode_daemon_str str = {
            *daemons,
            apiHandle,
            GetBinderName()
        };
        wrap_json_object_for_all(j_daemons, decode_daemons_cb, &str);
    }

    return 0;
}

int trace_exchange(AFB_ApiT apiHandle, DAEMON_T* svr, DAEMON_T* cli)
{
    int rc;
    json_object *j_response, *j_query;

    if (svr == NULL || cli == NULL) {
        return -1;
    }

    wrap_json_pack(&j_query, "{s:i, s:{s:s}}", "pid", svr->pid, "add",
        "request", "common");
    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "trace", j_query,
             &j_response))
        < 0) {
        AFB_ApiError(apiHandle, "ERROR trace %d result: %s", svr->pid,
            json_object_to_json_string(j_response));
        return rc;
    }

    wrap_json_pack(&j_query, "{s:i}", "pid", cli->pid);
    if ((rc = AFB_ServiceSync(apiHandle, SRV_SUPERVISOR_NAME, "trace", j_query,
             &j_response))
        < 0) {
        AFB_ApiError(apiHandle, "ERROR trace %d result: %s", cli->pid,
            json_object_to_json_string(j_response));
        return rc;
    }

    return 0;
}

int supervisor_init(void)
{
    return 0;
}
