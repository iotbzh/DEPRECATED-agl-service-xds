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
#pragma once

#include <stdbool.h>
#include "wrap-json.h"

#define SRV_SUPERVISOR_NAME "supervisor"

// FIXME Use chained list instead of static array
#define MAX_CLIENTS 32
#define MAX_SERVERS 32
#define MAX_DAEMONS 1024


typedef struct daemon
{
    int pid;
    const char* name;
    bool isServer;
    bool isClient;
    //char *ws_clients[MAX_CLIENTS];
    //char *ws_servers[MAX_SERVERS];
    json_object *ws_clients;
    json_object *ws_servers;
    json_object *config;
    json_object *apis;
} DAEMON_T;

typedef struct daemons_result_
{
    int count;
    DAEMON_T *daemons[MAX_DAEMONS];
} DAEMONS_T;


extern int getDaemons(AFB_ApiT apiHandle, DAEMONS_T **daemons);
extern int trace_exchange(AFB_ApiT apiHandle, DAEMON_T *svr, DAEMON_T *cli, const char *level);
extern int supervisor_init(void);
