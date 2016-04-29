#!/bin/bash

VERSION="2.7.9"
if [ $# -eq 1 ]; then
	VERSION="$1"
fi

cd /tmp
wget -qc http://python.org/ftp/python/${VERSION}/Python-${VERSION}.tgz
tar zxf Python-${VERSION}.tgz
cd Python-${VERSION}
./configure --prefix=/opt/python
make
make install
curl http://peak.telecommunity.com/dist/ez_setup.py | /opt/python/bin/python
PYTHONDONTWRITEBYTECODE='' /opt/python/bin/easy_install pip
/opt/python/bin/pip install virtualenv
