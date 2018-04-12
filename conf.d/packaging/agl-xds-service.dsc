Format: 1.0
Source: agl-xds-service
Binary: agl-xds-service-bin
Architecture: any
Version: 2.0-0
Maintainer: Iot-Team <secretaria@iot.bzh>
Standards-Version: 3.8.2
Homepage: https://github.com/iotbzh/xds-service
Build-Depends: debhelper (>= 5),
 pkg-config,
 cmake,
 gcc,
 g++,
 libjson-c-dev ,
 libsystemd-dev  (>= 222),
 agl-app-framework-binder-dev ,
 agl-libmicrohttpd-dev  (>= 0.9.55)
Debtransform-Tar: agl-xds-service_1.0.orig.tar.gz
