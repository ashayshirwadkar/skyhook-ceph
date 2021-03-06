# https://raw.githubusercontent.com/cruzdb/zlog/master/docker/ci/micro-osd.sh
#    Copyright (C) 2013,2014 Loic Dachary <loic@dachary.org>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
set -e
set -u

DIR=$1

# get rid of process and directories leftovers
pkill ceph-mon || true
pkill ceph-osd || true
rm -fr $DIR

# cluster wide parameters
mkdir -p ${DIR}/log
cat >> $DIR/ceph.conf <<EOF
[global]
fsid = $(uuidgen)
osd crush chooseleaf type = 0
run dir = ${DIR}/run
auth cluster required = none
auth service required = none
auth client required = none
osd pool default size = 1
EOF
export CEPH_ARGS="--conf ${DIR}/ceph.conf"

# single monitor
MON_DATA=${DIR}/mon
mkdir -p $MON_DATA

cat >> $DIR/ceph.conf <<EOF
[mon.0]
log file = ${DIR}/log/mon.log
chdir = ""
mon cluster log file = ${DIR}/log/mon-cluster.log
mon data = ${MON_DATA}
mon addr = 127.0.0.1
EOF

ceph-mon --id 0 --mkfs --keyring /dev/null
touch ${MON_DATA}/keyring
ceph-mon --id 0 

# single osd
OSD_DATA=${DIR}/osd
mkdir ${OSD_DATA}

cat >> $DIR/ceph.conf <<EOF
[osd.0]
log file = ${DIR}/log/osd.log
chdir = ""
osd data = ${OSD_DATA}
osd journal = ${OSD_DATA}.journal
osd journal size = 100
osd objectstore = memstore
osd class load list = lock log numops refcount replica_log statelog timeindex user version tabular
EOF

OSD_ID=$(ceph osd create)
ceph osd crush add osd.${OSD_ID} 1 root=default host=localhost
ceph-osd --id ${OSD_ID} --mkjournal --mkfs
ceph-osd --id ${OSD_ID}

# check that it works
rados mkpool rbd
rados --pool rbd put group /etc/group
rados --pool rbd get group ${DIR}/group
diff /etc/group ${DIR}/group
ceph osd tree

export CEPH_CONF="${DIR}/ceph.conf"
