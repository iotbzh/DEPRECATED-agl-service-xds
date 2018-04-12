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
#include "xds-service-api.h"
#include "supervisor-service.h"
#include "xds-service-apidef.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SRV_SUPERVISOR_NAME "supervisor"
#define SRV_HARVESTER_NAME "harvester"

typedef struct metric_t {
    char* name;
    json_object* data;
    struct timespec timestamp;
} METRIC_T;

void list(struct afb_req request)
{
    json_object *result, *item = NULL;
    DAEMONS_T* daemons = NULL;

    getDaemons(&daemons);
    if (daemons == NULL) {
        afb_req_fail(request, "failed", "");
        return;
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
    afb_req_success(request, result, NULL);
}

void trace(struct afb_req request)
{
    int rc;
    json_object* req_args = afb_req_json(request);
    json_object* result = NULL;
    DAEMONS_T* daemons = NULL;
    const char* ws_name;
    const char* wsn;

    if (wrap_json_unpack(req_args, "{s:?s}", "ws", &ws_name)) {
        afb_req_fail(request, "Failed", "Error processing arguments.");
        return;
    }
    AFB_INFO("Trace ws: %s", ws_name);

    getDaemons(&daemons);
    if (daemons == NULL || daemons->count <= 0) {
        afb_req_fail(request, "failed", "No daemon found");
    }

    // search server and client pid
    DAEMON_T *pid_s = NULL, *pid_c = NULL;
    for (int i = 0; i < daemons->count; i++) {
        AFB_DEBUG("_DEBUG_ svr %s",
            json_object_to_json_string(daemons->daemons[i]->ws_servers));
        AFB_DEBUG("_DEBUG_ cli %s",
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
            if ((rc = trace_exchange(pid_s, pid_c)) < 0) {
                afb_req_fail_f(request, "failed", "Trace error %d", rc);
            }
            break;
        }
    }

    if (pid_s == NULL || pid_c == NULL) {
        afb_req_fail(request, "failed", "Cannot determine Server or Client");
        return;
    }

    afb_req_success_f(request, result, "Tracing Server pid=%d <-> Client pid=%d",
        pid_s->pid, pid_c->pid);
}

static int harvester_post_data(METRIC_T* metric)
{

    int rc;
    json_object *j_res, *j_query;

    if (!metric->timestamp.tv_sec && !metric->timestamp.tv_nsec) {
        clock_gettime(CLOCK_MONOTONIC, &metric->timestamp);
    }

    rc = wrap_json_pack(&j_query, "{s:s s:i s:{ s:s s:o s:i } }", "host",
        "localhost", "port", 8086, "metric", "name", metric->name,
        "value", metric->data, "timestamp",
        metric->timestamp.tv_sec);
    if (rc < 0) {
        AFB_ERROR("Error packing metric, rc=%d", rc);
        return rc;
    }

    AFB_DEBUG("%s write: %s", SRV_HARVESTER_NAME,
        json_object_to_json_string(j_query));

    rc = afb_service_call_sync(SRV_HARVESTER_NAME, "write", j_query, &j_res);
    if (rc < 0) {
        AFB_ERROR("Error %s write : rc=%d, j_res=%s", SRV_HARVESTER_NAME, rc,
            json_object_to_json_string(j_res));
        return rc;
    }
    return 0;
}

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

void auth(struct afb_req request)
{
    afb_req_session_set_LOA(request, 1);
    afb_req_success(request, NULL, NULL);
}

int init()
{

#if 0 // DEBUG
  DAEMONS_T *daemons = NULL;
  getDaemons(&daemons);

  if (daemons) {
    if (daemons->count) {
      AFB_DEBUG("_DEBUG_ daemons->count %d", daemons->count);
      for (int i = 0; i < daemons->count; i++) {
        AFB_DEBUG("pid %d : isServer %d, isClient %d, %s %s",
               daemons->daemons[i]->pid, daemons->daemons[i]->isServer,
               daemons->daemons[i]->isClient,
               json_object_to_json_string(daemons->daemons[i]->ws_servers),
               json_object_to_json_string(daemons->daemons[i]->ws_clients));
      }
      free(daemons);
    } else {
      AFB_DEBUG("_DEBUG_ no dameons detected !");
    }
  }
#endif

    supervisor_service_init();

    return 0;
}
