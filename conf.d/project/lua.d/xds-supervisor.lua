--[[
  Copyright (C) 2018 "IoT.bzh"
  Author Sebastien Douheret <sebastien@iot.bzh>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.


  NOTE: strict mode: every global variables should be prefixed by '_'
--]]
-- return serialised version of printable table
function ToJson(o)
    if type(o) == "table" then
        local s = "{"
        local i = 0
        for k, v in pairs(o) do
            if (i > 0) then
                s = s .. ","
            end
            i = i + 1
            if type(k) ~= "number" then
                k = '"' .. k .. '"'
            end
            s = s .. k .. ":" .. ToJson(v)
        end
        return s .. "}"
    else
        return '"' .. tostring(o) .. '"'
    end
end

function _trace_hello_events_(source, args, event)
    -- TODO nothing for now
end

function _trace_Async_CB(source, response, context)
    --AFB:debug(source, "--InLua-- _trace_Async_CB response=%s context=%s\n", Dump_Table(response), Dump_Table(context))

    if (response["request"]["status"] ~= "success") then
        AFB:error(source, "--InLua-- _trace_Async_CB response=%s context=%s", Dump_Table(response), Dump_Table(context))
        return
    end
end

function _trace_events_(source, args, event)
    --AFB:notice(source, "--InLua-- ENTER _trace_events_ event=%s\n", Dump_Table(event))

    local query = {
        ["host"] = "localhost",
        ["port"] = 8086,
        ["metric"] = {
            {
                ["name"] = "xds/supervisor/trace",
                ["metadata"] = {
                    ["identity"] = "xds_supervisor",
                    ["tag"] = event["tag"]
                },
                ["values"] = {
                    ["id"] = event["id"]
                },
                ["timestamp"] = event["time"]
            }
        }
    }

    if event.request then
        local request = event.request
        -- Filter out some traces
        -- if (request.action == "begin" and request.action == "end" and request.action == "json") then
        --     AFB:debug(source, "--InLua-- _trace_events_ IGNORED event=%s\n", Dump_Table(event))
        --     return
        -- end
        AFB:notice(source, ">>> PROCESS request %s", request)
        query.metric[1].metadata.type = "request"
        query.metric[1].metadata.api = request.api
        query.metric[1].metadata.verb = request.verb
        query.metric[1].metadata.action = request.action
        query.metric[1].metadata.session = request.session
        query.metric[1].metadata.req_index = tostring(request.index)
        if event.data then
            local dd = ToJson(event.data)
            query.metric[1].values.data = dd
            query.metric[1].values.data_bytes = string.len(dd)
        end
    elseif event.event then
        local evt = event.event
        AFB:notice(source, ">>> PROCESS event %s", evt)
        query.metric[1].metadata.type = "event"
        query.metric[1].metadata.id = evt.id
        query.metric[1].metadata.name = evt.name
        query.metric[1].metadata.action = evt.action
        if event.data then
            local dd = ToJson(event.data)
            query.metric[1].values.data = dd
            query.metric[1].values.data_bytes = string.len(dd)
        end
    else
        AFB:warning(source, "--InLua-- UNKNOWN _trace_events_ event type: %s\n", Dump_Table(event))
        return
    end

    AFB:debug(source, "CALL harvester write query=%s", Dump_Table(query))
    local err, response = AFB:service(source, "harvester", "write", query, "_trace_Async_CB", query)

    -- FIXME SEB : still true ?
    -- Note: in current version controls only return a status. Also we may safely ignore API response
    if (err) then
        AFB:error(source, "--LUA:_trace_events_ harvester write refuse response=%s", response)
        return 1
    end

    return 0
end
