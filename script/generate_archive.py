#! /usr/bin/python3

import argparse
import base64
from getpass import getpass
import hashlib
import psycopg2
import random
import sys
from time import time
from uuid import uuid4

parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("-d", "--dbname", dest="database", default="storiqone", help="Specify the name of database to connect to")
parser.add_argument("-F", "--nbFiles", dest="nbFiles", default="256-1024", help="Specify the number of files to create into archive")
parser.add_argument("-h", "--host", dest="hostname", help="Specify the host name of the machine on which the server is running")
parser.add_argument("-p", "--port", dest="port", type=int, help="Specifies the TCP port or the local Unix-domain socket file extension on which the server is listening")
parser.add_argument("-P", "--pool", dest="pool", type=int, help="Specifies the pool id")
parser.add_argument("-U", "--username", dest="username", default="storiq", help="Connect to the database as the user username instead of the default")
parser.add_argument("-W", "--password", dest="password", default=False, action='store_true', help="Force to prompt for a password before connecting to a database")

args = parser.parse_args()


def getMinMaxNbFiles():
    if args.nbFiles.find('-') < 0:
        return (0, int(args.nbFiles))
    (min, max) = args.nbFiles.split('-')
    return (int(min), int(max))

(minNbFiles, maxNbFiles) = getMinMaxNbFiles()
nbFiles = random.randint(minNbFiles, maxNbFiles)

if args.pool is None:
    print("Error, pool id is required")
    sys.exit(1)

password = None
if args.password:
    try:
        password = getpass()
    except:
        print("Catch exception while getting password")


db_config = {
    'dbname': args.database
}
if args.hostname is not None:
    db_config['host'] = args.hostname
if args.port is not None:
    db_config['port'] = args.port
if args.username is not None:
    db_config['user'] = args.username
if password is not None:
    db_config['password'] = password

print("Connectiong to database '%s'…" % args.database, end="", flush=True)

try:
    db_conn = psycopg2.connect(**db_config)
    db_conn.autocommit = False
except:
    print(" Failed")
    sys.exit()

print(" Connected")

db_conn.autocommit = False
cur = db_conn.cursor()

print("Get a media from pool id %d…" % args.pool, end="", flush=True)

cur.execute("SELECT id FROM media WHERE pool = %d LIMIT 1" % (args.pool))
if cur.rowcount == 1:
    media = cur.fetchone()[0]
    print(" found, will use media(%d)" % (media))
else:
    print(" no media found")
    sys.exit()


print("Get a user from databse…", end="", flush=True)

cur.execute("SELECT id FROM users LIMIT 1")
if cur.rowcount == 1:
    user = cur.fetchone()[0]
    print(" found, will use user(%d)" % (media))
else:
    print(" no user found")
    sys.exit()


print("Get a path…", end="", flush=True)

cur.execute("SELECT id FROM selectedfile WHERE path = '/' LIMIT 1")
if cur.rowcount == 1:
    parent = cur.fetchone()[0]
    print(" found, will use path(%d)" % (parent))
else:
    print("Not found, create new one…", end="", flush=True)

    cur.execute("INSERT INTO selectedfile(path) VALUES ('/') RETURNING id")
    if cur.rowcount == 1:
        parent = cur.fetchone()[0]
        print(" created, will use path(%d)" % (parent))
    else:
        print(" not created")
        sys.exit()


fd = open('/dev/urandom', 'rb')
def generateRandomName(length):
    data = fd.read(length)
    return base64.b64encode(hashlib.sha1(data).digest()).decode('utf-8')

def createArchive():
    uuid = uuid4()
    name = generateRandomName(64)

    print("Create new archive('%s', '%s')…" % (uuid, name), end="", flush=True)

    cur.execute("INSERT INTO archive(uuid, name, creator, owner, status) VALUES ('%s', '%s', %d, %d, 'complete') RETURNING id" % (uuid, name, user, user))
    if cur.rowcount == 1:
        archive = cur.fetchone()[0]
        print(" ok, new archive id(%d)" % (archive))
        return archive
    else:
        print(" no archive created")
        sys.exit()

totalSize = 0
def createFile(volume, parent):
    global totalSize

    name = '/' + generateRandomName(64) + '/' + generateRandomName(64);
    mimetype = generateRandomName(8)
    size = random.randint(0, 1073741824)
    totalSize += size

    cur.execute("INSERT INTO archivefile VALUES (DEFAULT, '%s', 'regular file', '%s', 1000, 'storiq', 1000, 'storiq', 420, NOW(), NOW(), %d, %d) RETURNING id" % (name, mimetype, size, parent))
    if cur.rowcount == 1:
        file = cur.fetchone()[0]
    else:
        print(" no file created")
        sys.exit()

    cur.execute("INSERT INTO archivefiletoarchivevolume VALUES (%d, %d, 0, NOW(), NULL, DEFAULT, NULL) RETURNING *" % (volume, file))
    if cur.rowcount < 1:
        print(" no file created")
        sys.exit()

def createVolume(archive, media):
    print("Create new volume('%d', '%d')…" % (archive, media), end="", flush=True)

    cur.execute("INSERT INTO archivevolume(starttime, archive, media) VALUES (NOW(), %d, %d) RETURNING id" % (archive, media))
    if cur.rowcount == 1:
        volume = cur.fetchone()[0]
        print(" ok, new archive id(%d)" % (archive))
        return volume
    else:
        print(" no volume created")
        sys.exit()

def updateVolume(volume, totalSize):
    print("\nUpdate volume('%d', '%d')…" % (volume, totalSize), end="", flush=True)

    cur.execute("UPDATE archivevolume SET endtime = NOW(), size = %d WHERE id = %d RETURNING id" % (totalSize, volume))

    if cur.rowcount == 1:
        print(" ok")
    else:
        print(" failed")

archive = createArchive()
volume = createVolume(archive, media)

start = int(time())

for counter in range(1, nbFiles):
    current = int(time())
    if start != current:
        print("\rcreate file (%d / %d)" % (counter, nbFiles), end="", flush=True)
        start = current

    createFile(volume, parent)

updateVolume(volume, totalSize)

db_conn.commit()

