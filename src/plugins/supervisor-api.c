/*
 * Copyright (C) 2018 "IoT.bzh"
 * Author "Sebastien Douheret" <sebastien@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ctl-plugin.h"
#include "supervisor-api.h"
#include "supervisor.h"
#include "wrap-json.h"

CTLP_CAPI_REGISTER("supervisor");

CTLP_ONLOAD(plugin, ret)
{
    return 0;
}

CTLP_CAPI(list, source, argsJ, eventJ)
{
    json_object *result, *item = NULL;
    DAEMONS_T* daemons = NULL;

    getDaemons(source->api, &daemons);
    if (daemons == NULL) {
        AFB_ApiError(source->api, "failed");
        return ERROR;
    }

    result = json_object_new_array();

    for (int i = 0; i < daemons->count; i++) {
        wrap_json_pack(&item, "{si ss sb sb so so so}",
            "pid", daemons->daemons[i]->pid,
            "name", daemons->daemons[i]->name,
            "isServer", daemons->daemons[i]->isServer,
            "isClient", daemons->daemons[i]->isClient,
            "ws_servers", daemons->daemons[i]->ws_servers,
            "ws_clients", daemons->daemons[i]->ws_clients,
            "apis", daemons->daemons[i]->apis);
        //, "config", daemons->daemons[i]->config);
        json_object_array_add(result, item);
    }
    AFB_ReqSucess(source->request, result, NULL);
    return 0;
}

CTLP_CAPI(trace, source, argsJ, eventJ)
{
    int rc;
    json_object* result = NULL;
    DAEMONS_T* daemons = NULL;
    const char* ws_name;
    const char* wsn;

    if (wrap_json_unpack(argsJ, "{s:?s}", "ws", &ws_name)) {
        AFB_ReqFail(source->request, "Failed", "Error processing arguments.");
        return ERROR;
    }
    AFB_ApiNotice(source->api, "Trace ws: %s", ws_name);

    getDaemons(source->api, &daemons);
    if (daemons == NULL || daemons->count <= 0) {
        AFB_ReqFail(source->request, "failed", "No daemon found");
    }

    // search server and client pid
    DAEMON_T *pid_s = NULL, *pid_c = NULL;
    for (int i = 0; i < daemons->count; i++) {
        AFB_ApiDebug(source->api, "_DEBUG_ svr %s",
            json_object_to_json_string(daemons->daemons[i]->ws_servers));
        AFB_ApiDebug(source->api, "_DEBUG_ cli %s",
            json_object_to_json_string(daemons->daemons[i]->ws_clients));

        json_object* ws_servers = daemons->daemons[i]->ws_servers;
        for (int j = 0; j < json_object_array_length(ws_servers); j++) {

            wsn = json_object_get_string(json_object_array_get_idx(ws_servers, j++));
            if (wsn && strstr(wsn, ws_name) != NULL) {
                pid_s = daemons->daemons[i];
                break;
            }
        }

        json_object* ws_clients = daemons->daemons[i]->ws_clients;
        for (int j = 0; j < json_object_array_length(ws_clients); j++) {
            wsn = json_object_get_string(json_object_array_get_idx(ws_clients, j++));
            if (wsn && strstr(wsn, ws_name) != NULL) {
                pid_c = daemons->daemons[i];
                break;
            }
        }

        if (pid_s != NULL && pid_c != NULL) {
            if ((rc = trace_exchange(source->api, pid_s, pid_c)) < 0) {
                AFB_ReqFailF(source->request, "failed", "Trace error %d", rc);
            }
            break;
        }
    }

    if (pid_s == NULL || pid_c == NULL) {
        AFB_ReqFail(source->request, "failed", "Cannot determine Server or Client");
        return ERROR;
    }

    AFB_ReqSucessF(source->request, result, "Tracing Server pid=%d <-> Client pid=%d", pid_s->pid, pid_c->pid);

    return 0;
}

/* SEB TODO
void xds_event_cb(const char* evtname, json_object* j_event)
{
    int rc;
    METRIC_T metric;
    const char* type = NULL;
    struct json_object* request = NULL;

    AFB_NOTICE("RECV Event %s : %s", evtname,
        json_object_to_json_string(j_event));

    if (strcmp(evtname, "supervisor/trace") != 0) {
        return;
    }

    if ((rc = wrap_json_unpack(j_event, "{s:?s}", "type", &type)) < 0) {
        AFB_ERROR("Cannot decode event type");
        return;
    }

    if (strcmp(type, "request") == 0) {

        if (!json_object_object_get_ex(j_event, "request", &request)) {
            AFB_ERROR("Cannot decode event request");
            return;
        }
        metric.name = "trace";
        metric.data = request;

        rc = harvester_post_data(&metric);
        if (rc < 0) {
            AFB_ERROR("ERROR harvester_post_data: rc %d", rc);
        }
    }
}
*/
