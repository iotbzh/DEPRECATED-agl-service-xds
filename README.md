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
#setup your build environement
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

Here are commands used to setup some bindings in order to test on `xds-service` natively on a Linux host:

```bash
afs-supervisor --port 1712 --token HELLO --ws-server=unix:/tmp/supervisor -vv

cd $ROOT_DIR/app-framework-binder
afb-daemon -t '' -p 5555 -M --roothttp test --ws-server unix:ave --name test_server -vv
afb-daemon -t '' -p 4444 -M --roothttp test --no-ldpaths --ws-client unix:ave --name test_client -vv

cd $ROOT_DIR/agl-service-harvester
afb-daemon --port=1234 --workdir=./build/package --ldpaths=lib --roothttp=htdocs  --token= --tracereq=common -vv --ws-server unix:/tmp/harvester

cd $ROOT_DIR/agl-service-xds
./conf.d/autobuild/linux/autobuild build
afb-daemon --port=5678 --name=afb-xds --workdir=./build/package --ldpaths=lib --roothttp=htdocs  --token=1977 --ws-client=unix:/tmp/supervisor --ws-client=unix:/tmp/harvester -vv
```

afb-client-demo -H 'localhost:5678/api?token=1977&uuid=magic' xds list

## Deploy

### AGL

TBD
