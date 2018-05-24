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
#include "filescan-utils.h"

#define SRV_HARVESTER_NAME "harvester"

#define META_SOURCENAME GetBinderName();
#define META_IDENTITY "" // FIXME

#ifndef ERROR
  #define ERROR -1
#endif

typedef struct metric_t {
    char* name;
    json_object* data;
    struct timespec timestamp;
} METRIC_T;
