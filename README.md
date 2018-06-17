# agl-service-xds

An AGL binding used to control collected data. Those data may come from
`agl-low-collector` or from AGL `supervision`.

**UNDER DEVELOPMENT - this binding is not fully functional, it's a proof of concept for now**

## Setup

```bash
git clone --recursive https://github.com/iotbzh/agl-server-xds
cd agl-server-xds
```

## Build  for AGL

```bash
#setup your build environment
. /xdt/sdk/environment-setup-aarch64-agl-linux
#build your application
./conf.d/autobuild/agl/autobuild package
```

## Build for 'native' Linux distros (Fedora, openSUSE, Debian, Ubuntu, ...)

```bash
./conf.d/autobuild/linux/autobuild package
```

You can also use binary package from OBS: [opensuse.org/LinuxAutomotive][opensuse.org/LinuxAutomotive]

## Test

### Native setup

Here are commands used to build and setup some AGL services/bindings in order to test on `xds` service natively on a Linux host.

#### Build

```bash
cd $ROOT_DIR/agl-service-xds
./conf.d/autobuild/linux/autobuild build
```

#### Run services/bindings

```bash
# Setup supervisor to supervise AGL system
afs-supervisor --port 1712 --token HELLO --ws-server=unix:/tmp/supervisor -vv

# Start harverster to save data in a TSDB
cd $ROOT_DIR/agl-service-harvester
afb-daemon --port=1234 --name=afb-harvester --workdir=./build/package --ldpaths=lib --roothttp=htdocs  --token= --tracereq=common --ws-server unix:/tmp/harvester -vv

# Start XDS collector to control supervisor
cd $ROOT_DIR/agl-service-xds
afb-daemon --port=5678 --name=afb-xds --workdir=./build/package --ldpaths=lib --roothttp=htdocs  --token= --ws-client=unix:/tmp/supervisor --ws-client=unix:/tmp/harvester -vvv
```

**Example 1** : Demo based on simple helloworld AGL service

``` bash
cd $ROOT_DIR/app-framework-binder
afb-daemon -t '' -p 5555 -M --roothttp test --ws-server unix:ave --name test_server -vvv
afb-daemon -t '' -p 4444 -M --roothttp test --no-ldpaths --ws-client unix:ave --name test_client -vvv

# Get bindings list
afb-client-demo --raw 'localhost:5678/api?token=1977&uuid=magic' xds list | jq .response[].pid

# Request monitoring of socket "unix:ave"
afb-client-demo 'localhost:5678/api?token=HELLO&uuid=c' xds trace '{"ws":"unix:ave"}'

# Call ping verb of hello api (should be spy by supervision/monitoring)
afb-client-demo 'localhost:5555/api?token=toto&uuid=magic' hello ping

# Same call some other verbs of hello api
afb-client-demo 'localhost:5555/api?token=toto&uuid=magic' hello eventadd '{"tag":"x","name":"event"}'
afb-client-demo 'localhost:5555/api?token=toto&uuid=magic' hello eventsub '{"tag":"x"}'
afb-client-demo 'localhost:5555/api?token=toto&uuid=magic' hello eventpush '{"tag":"x","data":true}'

# Dump data from TSDB
influx -database 'agl-garner' -execute 'show series'
influx -database 'agl-garner' -execute 'select * from "xds/supervisor/trace"'
```

**Example 2**: Demo based on AGL mockup services

```bash
# Monitor AGL mockup services
cd $ROOT_DIR/agl-services-mockup && start_agl_mockup.sh 1
# ... at this point script stop and wait user...

# In another shell, execute following commands to request monitoring of can_emul and gps_emul sockets
afb-client-demo 'localhost:5678/api?token=HELLO&uuid=c' xds trace/start '{"ws":"can_emul"}'
afb-client-demo 'localhost:5678/api?token=HELLO&uuid=c' xds trace/start '{"ws":"gps_emul"}'

# Press any key in 1 shell to continue start_agl_mockup.sh script

# Dump data from TSDB
influx -database 'agl-garner' -execute 'select * from "xds/supervisor/trace"'

# Drop data
# influx -database 'agl-garner' -execute 'drop measurement "xds/supervisor/trace"'
```

## Deploy

### AGL

TBD
