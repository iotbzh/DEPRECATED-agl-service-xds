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
#include <time.h>
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

    AFB_ApiInfo(source->api, "Build response (nb daemons %d)", daemons->count);
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

    AFB_ApiInfo(source->api, "Send response");
    AFB_ReqSuccess(source->request, result, NULL);
    return 0;
}

CTLP_CAPI(trace, source, argsJ, queryJ)
{
    int rc;
    json_object* result = NULL;
    DAEMONS_T* daemons = NULL;
    const char* ws_name = "";
    const char* level = NULL;
    pid_t pid = -1;

    if (wrap_json_unpack(queryJ, "{s:?i s:?s s:?s}",
            "pid", &pid, "ws", &ws_name, "level", &level)) {
        AFB_ReqFail(source->request, "Failed", "Error processing arguments.");
        return ERROR;
    }

    if (pid == -1 && strlen(ws_name) == 0) {
        AFB_ReqFail(source->request, "failed", "one of pid or ws parameter must be set");
        return ERROR;
    }

    AFB_ApiDebug(source->api, "Trace pid: %d ws: %s", pid, ws_name);

    getDaemons(source->api, &daemons);
    if (daemons == NULL || daemons->count <= 0) {
        AFB_ReqFail(source->request, "failed", "No daemon found");
        return ERROR;
    }

    // search server and client pid
    DAEMON_T *pid_s = NULL, *pid_c = NULL;
    const char* wsn;
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
            if ((rc = trace_exchange(source->api, pid_s, pid_c, level)) < 0) {
                AFB_ReqFailF(source->request, "failed", "Trace error %d", rc);
            }
            break;
        }
    }

    if (pid_s == NULL || pid_c == NULL) {
        AFB_ReqFail(source->request, "failed", "Cannot determine Server or Client");
        return ERROR;
    }

    AFB_ReqSuccessF(source->request, result, "Tracing Server pid=%d <-> Client pid=%d", pid_s->pid, pid_c->pid);

    return 0;
}

uint64_t get_ts()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    return (uint64_t)(ts.tv_sec) * (uint64_t)1000000000 + (uint64_t)(ts.tv_nsec);
}

static void cb_harvester_write(void* closure, int status, struct json_object* result, struct afb_dynapi* dynapi)
{
    AFB_ApiDebug(dynapi, "SEB cb_harvester_write");
}

/*  SEB Cleanup if move in lua */
static int harvester_post_data(CtlSourceT* source, METRIC_T* metric)
{
    int rc;
    json_object *j_res, *j_query, *j_metric;

    if (!metric->timestamp) {
        metric->timestamp = get_ts();
    }

    // To be REWORK
    if (metric->dataType == SPVR_DATA_STRING) {
        rc = wrap_json_pack(&j_metric, "{s:s s:{s:s s:s} s:{s:s} s:I }",
            "name", metric->name,
            "metadata", "source", "my_source", "identity", source->uid,
            "values", "value", metric->data,
            "timestamp", metric->timestamp);
    } else if (metric->dataType == SPVR_DATA_INT) {
        rc = wrap_json_pack(&j_metric, "{s:s s:{s:s s:s} s:{s:i} s:I }",
            "name", metric->name,
            "metadata", "source", "my_source", "identity", source->uid,
            "values", "value", metric->data,
            "timestamp", metric->timestamp);
    } else {
        AFB_ApiError(source->api, "Unsupported dataType");
        return ERROR;
    }

    if (rc < 0) {
        AFB_ApiError(source->api, "Error packing metric, rc=%d", rc);
        return rc;
    }

    rc = wrap_json_pack(&j_query, "{s:s s:i s:o }", "host",
        "localhost", "port", 8086, "metric", j_metric);
    if (rc < 0) {
        AFB_ApiError(source->api, "Error packing query, rc=%d", rc);
        return rc;
    }

    AFB_ApiDebug(source->api, "%s write: %s", SRV_HARVESTER_NAME,
        json_object_to_json_string(j_query));

    /* SEB
    rc = AFB_ServiceSync(source->api, SRV_HARVESTER_NAME, "write", j_query, &j_res);
    if (rc < 0) {
        AFB_ApiError(source->api, "Error %s write : rc=%d, j_res=%s", SRV_HARVESTER_NAME, rc,
            json_object_to_json_string(j_res));
        return rc;
    }
*/
    AFB_ServiceCall(source->api, SRV_HARVESTER_NAME, "write", j_query, cb_harvester_write, &j_res);

    return 0;
}

CTLP_CAPI(tracing_events, source, argsJ, eventJ)
{
    int rc;
    METRIC_T metric = { 0 };
    const char* type = NULL;
    struct json_object* request = NULL;

    //struct signalCBT* ctx = (struct signalCBT*)source->context;

    AFB_ApiDebug(source->api, ">>> RECV Event uid %s : %s", source->uid,
        json_object_to_json_string(eventJ));

    if (strcmp(source->uid, "supervisor/trace") != 0) {
        AFB_ApiNotice(source->api, "WARNING: un-handle uid '%s'", source->uid);
        return 0;
    }

    if ((rc = wrap_json_unpack(eventJ, "{s:?s}", "type", &type)) < 0) {
        AFB_ApiError(source->api, "Cannot decode event type");
        return ERROR;
    }

    if (strcmp(type, "request") != 0) {
        AFB_ApiError(source->api, "Cannot retrieve request");
        return ERROR;
    }
    if (!json_object_object_get_ex(eventJ, "request", &request)) {
        AFB_ApiError(source->api, "Cannot decode event request");
        return ERROR;
    }

    // TODO: decode request and build trace

    metric.name = "trace";
    /* FIXME string KO
    metric.dataType = SPVR_DATA_STRING;
    metric.data = "test1234";
    */
    metric.dataType = SPVR_DATA_INT;
    int val = 54321;
    metric.data = &val;

    rc = harvester_post_data(source, &metric);
    if (rc < 0) {
        AFB_ApiError(source->api, "ERROR harvester_post_data: rc %d", rc);
        return rc;
    }

    return 0;
}
