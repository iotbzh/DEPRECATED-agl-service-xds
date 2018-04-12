
static const char _afb_description_v2_xds_service[] =
    "{\"openapi\":\"3.0.0\",\"$schema\":\"http://iot.bzh/download/openapi/sch"
    "ema-3.0/default-schema.json\",\"info\":{\"description\":\"TBD - TODO\",\""
    "title\":\"xds-service\",\"version\":\"4.0\",\"x-binding-c-generator\":{\""
    "api\":\"xds-service\",\"version\":2,\"prefix\":\"\",\"postfix\":\"\",\"s"
    "tart\":null,\"onevent\":\"xds_event_cb\",\"init\":\"init\",\"scope\":\"\""
    ",\"private\":false}},\"servers\":[{}],\"components\":{\"schemas\":{\"afb"
    "-reply\":{\"$ref\":\"#/components/schemas/afb-reply-v2\"},\"afb-event\":"
    "{\"$ref\":\"#/components/schemas/afb-event-v2\"},\"afb-reply-v2\":{\"tit"
    "le\":\"Generic response.\",\"type\":\"object\",\"required\":[\"jtype\",\""
    "request\"],\"properties\":{\"jtype\":{\"type\":\"string\",\"const\":\"af"
    "b-reply\"},\"request\":{\"type\":\"object\",\"required\":[\"status\"],\""
    "properties\":{\"status\":{\"type\":\"string\"},\"info\":{\"type\":\"stri"
    "ng\"},\"token\":{\"type\":\"string\"},\"uuid\":{\"type\":\"string\"},\"r"
    "eqid\":{\"type\":\"string\"}}},\"response\":{\"type\":\"object\"}}},\"af"
    "b-event-v2\":{\"type\":\"object\",\"required\":[\"jtype\",\"event\"],\"p"
    "roperties\":{\"jtype\":{\"type\":\"string\",\"const\":\"afb-event\"},\"e"
    "vent\":{\"type\":\"string\"},\"data\":{\"type\":\"object\"}}}},\"x-permi"
    "ssions\":{\"list\":{\"permission\":\"urn:AGL:permission::platform:can:li"
    "st \"},\"trace\":{\"permission\":\"urn:AGL:permission::platform:can:trac"
    "e \"}},\"responses\":{\"200\":{\"description\":\"A complex object array "
    "response\",\"content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/c"
    "omponents/schemas/afb-reply\"}}}}}},\"paths\":{\"/auth\":{\"description\""
    ":\"Authenticate session to raise Level Of Assurance of the session\",\"g"
    "et\":{\"x-permissions\":{\"$ref\":\"#/components/x-permissions/list\"},\""
    "responses\":{\"200\":{\"$ref\":\"#/components/responses/200\"}}}},\"/lis"
    "t\":{\"description\":\"list \",\"get\":{\"x-permissions\":{\"LOA\":1},\""
    "parameters\":[],\"responses\":{\"200\":{\"$ref\":\"#/components/response"
    "s/200\"}}}},\"/trace\":{\"description\":\"trace \",\"get\":{\"x-permissi"
    "ons\":{\"LOA\":1},\"parameters\":[{\"in\":\"query\",\"name\":\"ws\",\"re"
    "quired\":true,\"schema\":{\"type\":\"string\"}}],\"responses\":{\"200\":"
    "{\"$ref\":\"#/components/responses/200\"}}}}}}"
;

static const struct afb_auth _afb_auths_v2_xds_service[] = {
	{ .type = afb_auth_Permission, .text = "urn:AGL:permission::platform:can:list " }
};

 void auth(struct afb_req req);
 void list(struct afb_req req);
 void trace(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_xds_service[] = {
    {
        .verb = "auth",
        .callback = auth,
        .auth = &_afb_auths_v2_xds_service[0],
        .info = "Authenticate session to raise Level Of Assurance of the session",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "list",
        .callback = list,
        .auth = NULL,
        .info = "list ",
        .session = AFB_SESSION_LOA_1_V2
    },
    {
        .verb = "trace",
        .callback = trace,
        .auth = NULL,
        .info = "trace ",
        .session = AFB_SESSION_LOA_1_V2
    },
    {
        .verb = NULL,
        .callback = NULL,
        .auth = NULL,
        .info = NULL,
        .session = 0
	}
};

const struct afb_binding_v2 afbBindingV2 = {
    .api = "xds-service",
    .specification = _afb_description_v2_xds_service,
    .info = "TBD - TODO",
    .verbs = _afb_verbs_v2_xds_service,
    .preinit = NULL,
    .init = init,
    .onevent = xds_event_cb,
    .noconcurrency = 0
};

