#! /usr/bin/python3

import argparse
from datetime import datetime, timezone
from getpass import getpass
import json
import psycopg2
import sys
from time import time


parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("-d", "--dbname", dest="database", default="storiqone", help="Specify the name of database to connect to")
parser.add_argument("-f", "--filename", dest="filename", default=None, help="Read commands from the file filename, rather than standard input")
parser.add_argument("-h", "--host", dest="hostname", help="Specify the host name of the machine on which the server is running")
parser.add_argument("-p", "--port", dest="port", type=int, help="Specifies the TCP port or the local Unix-domain socket file extension on which the server is listening")
parser.add_argument("-U", "--username", dest="username", help="Connect to the database as the user username instead of the default")
parser.add_argument("-W", "--password", dest="password", default=False, action='store_true', help="Force to prompt for a password before connecting to a database")

args = parser.parse_args()


password = None
if args.password:
    try:
        password = getpass()
    except:
        print("Catch exception while getting password")

try:
    if args.filename is not None:
        print("Loading json (from %s)…" % args.filename, end="", flush=True)
        fd = open(args.filename, 'r')
    else:
        print("Loading json (from stdin)…", end="", flush=True)
        fd = sys.stdin
    archive = json.load(fd)
    fd.close()
except:
    print("Failed")
    raise

print("Ok")

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
    print("Failed")
    raise

print("Connected")

cur = db_conn.cursor()

last_display = 0
def display(message, force=False):
    global last_display

    if force:
        last_display = 0

    current = int(time())

    if last_display != current:
        print("\r%s…\033[K" % message, end="", flush=True)
        last_display = current

files = {}
selected_files = {}
try:
    display("Prepare queries")
    cur.execute("PREPARE insert_archive AS WITH u AS (SELECT $1::UUID, $2::TEXT, id, id, $4::archivestatus FROM users WHERE login = $3 LIMIT 1) INSERT INTO archive(uuid, name, owner, creator, status) SELECT * FROM u RETURNING id");

    cur.execute("PREPARE select_archive_metadata AS SELECT id FROM metadata WHERE id = $1 AND type = 'archive' AND key = $2 LIMIT 1")
    cur.execute("PREPARE update_archive_metadata AS UPDATE metadata SET value = to_json($3::TEXT::JSON)::JSONB WHERE id = $1 AND type = 'archive' AND key = $2")
    cur.execute("PREPARE insert_archive_metadata AS WITH a AS (SELECT owner FROM archive WHERE id = $1 LIMIT 1) INSERT INTO metadata(id, type, key, value, login) SELECT $1, 'archive', $2::TEXT, to_json($3::TEXT::JSON)::JSONB, owner FROM a")

    cur.execute("PREPARE insert_archivevolume AS INSERT INTO archivevolume(sequence, size, starttime, endtime, archive, media, mediaposition, jobrun) VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id")
    cur.execute("PREPARE select_media AS SELECT id FROM media WHERE uuid = $1 LIMIT 1")

    cur.execute("PREPARE select_checksum_by_name AS SELECT id FROM checksum WHERE name = $1 LIMIT 1")
    cur.execute("PREPARE select_checksumresult AS SELECT id FROM checksumresult WHERE checksum = $1 AND result = $2 LIMIT 1")
    cur.execute("PREPARE insert_checksumresult AS INSERT INTO checksumresult(checksum, result) VALUES ($1, $2) RETURNING id")
    cur.execute("PREPARE insert_archivefile_to_checksum AS INSERT INTO archivefiletochecksumresult VALUES ($1, $2)")
    cur.execute("PREPARE insert_archive_volume_to_checksum AS WITH avcr AS (SELECT $1::BIGINT AS archivevolume, $2::BIGINT AS checksumresult WHERE $1 || '@' || $2 NOT IN (SELECT archivevolume || '@' || checksumresult FROM archivevolumetochecksumresult)) INSERT INTO archivevolumetochecksumresult SELECT archivevolume, checksumresult FROM avcr")

    cur.execute("PREPARE select_selectedfile_by_name AS SELECT id FROM selectedfile WHERE path = $1 LIMIT 1")
    cur.execute("PREPARE insert_archive_file AS INSERT INTO archivefile(name, type, mimetype, ownerid, owner, groupid, groups, perm, ctime, mtime, size, parent) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id")

    cur.execute("PREPARE select_archivefile_metadata AS SELECT id FROM metadata WHERE id = $1 AND type = 'archivefile' AND key = $2 LIMIT 1")
    cur.execute("PREPARE update_archivefile_metadata AS UPDATE metadata SET value = to_json($3::TEXT::JSON)::JSONB WHERE id = $1 AND type = 'archivefile' AND key = $2")
    cur.execute("PREPARE insert_archivefile_metadata AS WITH a AS (SELECT owner FROM archive WHERE id = $1 LIMIT 1) INSERT INTO metadata(id, type, key, value, login) SELECT $1, 'archivefile', $2::TEXT, to_json($3::TEXT::JSON)::JSONB, owner FROM a")

    # cur.execute("PREPARE insert_archivefiletoarchivevolume AS WITH afav AS (SELECT $1:: BIGINT AS archivevolume, $2::BIGINT AS archivefile, $3::BIGINT AS blocknumber, $4::TIMESTAMPTZ AS archivetime WHERE $1 || '@' || $2 NOT IN (SELECT archivevolume || '@' || archivefile FROM archivefiletoarchivevolume)) INSERT INTO archivefiletoarchivevolume(archivevolume, archivefile, blocknumber, archivetime) SELECT archivevolume, archivefile, blocknumber, archivetime FROM afav")
    cur.execute("PREPARE insert_archivefiletoarchivevolume AS INSERT INTO archivefiletoarchivevolume(archivevolume, archivefile, blocknumber, archivetime) SELECT $1::BIGINT, $2::BIGINT, $3::BIGINT, $4::TIMESTAMPTZ WHERE NOT EXISTS (SELECT * FROM archivefiletoarchivevolume WHERE archivefile = $2 AND archivevolume = $1)")

    display("Create archive (%s)" % archive['name'], True)
    cur.execute("EXECUTE insert_archive('%s', '%s', '%s', '%s')" % (archive['uuid'], archive['name'], archive['creator'], archive['status']))
    (archive['id']) = cur.fetchone()[0]

    if archive['metadata'] is not None:
        for meta in archive['metadata'].keys():
            display("Update metadata (%s) of archive (%s)" % (meta, archive['name']))

            value = json.dumps(archive['metadata'][meta])

            cur.execute("EXECUTE select_archive_metadata(%d, '%s')" % (archive['id'], meta))
            if cur.rowcount == 1:
                cur.execute("EXECUTE update_archive_metadata(%d, '%s', '%s')" % (archive['id'], meta, value))
            else:
                cur.execute("EXECUTE insert_archive_metadata(%d, '%s', '%s')" % (archive['id'], meta, value))

    for volume in archive['volumes']:
        if volume['job'] is None:
            continue

        display("Create volume #%d/%d of archive (%s)" % (volume['sequence'], len(archive['volumes']), archive['name']))

        cur.execute("EXECUTE select_media('%s')" % (volume['media']['uuid']))
        (volume['media']['id']) = cur.fetchone()[0]

        start_time = datetime.utcfromtimestamp(volume['start time']).replace(tzinfo=timezone.utc).astimezone(tz=None)
        end_time = datetime.utcfromtimestamp(volume['end time']).replace(tzinfo=timezone.utc).astimezone(tz=None)

        cur.execute("EXECUTE insert_archivevolume(%d, %d, '%s', '%s', %d, %d, %d, %d)" % (volume['sequence'], volume['size'], start_time.strftime('%F %T%Z'), end_time.strftime('%F %T%Z'), archive['id'], volume['media']['id'], volume['media position'], volume['job']['id']))
        (volume['id']) = cur.fetchone()[0]

        for checksum in volume['checksums']:
            display("Add checksum to volume #%d of archive (%s)" % (volume['sequence'], archive['name']))
            value = volume['checksums'][checksum]

            cur.execute("EXECUTE select_checksum_by_name('%s')" % checksum)
            (checksum_id) = cur.fetchone()[0]

            cur.execute("EXECUTE select_checksumresult(%d, '%s')" % (checksum_id, value))
            if cur.rowcount == 1:
                (checksumresult_id) = cur.fetchone()[0]
            else:
                cur.execute("EXECUTE insert_checksumresult(%d, '%s')" % (checksum_id, value))
                (checksumresult_id) = cur.fetchone()[0]

            cur.execute("EXECUTE insert_archive_volume_to_checksum(%d, '%s')" % (volume['id'], checksumresult_id))

        i_file = 0
        total_files = len(volume['files'])
        for ptr_file in volume['files']:
            file = ptr_file['file']

            if file['selected path'] not in selected_files:
                display("Find selected path (%s)" % (file['selected path']))
                cur.execute("EXECUTE select_selectedfile_by_name('%s')" % file['selected path'])
                (selected_files[file['selected path']]) = cur.fetchone()[0]

            if file['path'] not in files:
                display("Insert archive file #%d/%d (%s)" % (i_file, total_files, file['path']))

                ctime = datetime.utcfromtimestamp(file['create time']).replace(tzinfo=timezone.utc).astimezone(tz=None)
                mtime = datetime.utcfromtimestamp(file['modify time']).replace(tzinfo=timezone.utc).astimezone(tz=None)

                cur.execute("EXECUTE insert_archive_file('%s', '%s', '%s', %d, '%s', %d, '%s', %d, '%s', '%s', %d, %d)" % (file['path'], file['type'], file['mime type'], file['owner id'], file['owner'], file['group id'], file['group'], file['permission'], ctime.strftime('%F %T%Z'), mtime.strftime('%F %T%Z'), file['size'], selected_files[file['selected path']]))
                (file['id']) = cur.fetchone()[0]
                files[file['path']] = file['id']

                if file['checksums'] is not None:
                    display("Add checksum to file #%d/%d (%s)" % (i_file, total_files, file['path']))
                    for checksum in file['checksums']:
                        value = volume['checksums'][checksum]

                        cur.execute("EXECUTE select_checksum_by_name('%s')" % checksum)
                        (checksum_id) = cur.fetchone()[0]

                        cur.execute("EXECUTE select_checksumresult(%d, '%s')" % (checksum_id, value))
                        if cur.rowcount == 1:
                            (checksumresult_id) = cur.fetchone()[0]
                        else:
                            cur.execute("EXECUTE insert_checksumresult(%d, '%s')" % (checksum_id, value))
                            (checksumresult_id) = cur.fetchone()[0]

                        cur.execute("EXECUTE insert_archivefile_to_checksum(%d, '%s')" % (file['id'], checksumresult_id))

                if file['metadata'] is not None:
                    display("Update metadata (%s) of archive file #%d/%d (%s)" % (meta, i_file, total_files, file['path']))
                    for meta in file['metadata']:
                        value = json.dumps(file['metadata'][meta])

                        cur.execute("EXECUTE select_archivefile_metadata(%d, '%s')" % (file['id'], meta))
                        if cur.rowcount == 1:
                            cur.execute("EXECUTE update_archivefile_metadata(%d, '%s', '%s')" % (file['id'], meta, value))
                        else:
                            cur.execute("EXECUTE insert_archivefile_metadata(%d, '%s', '%s')" % (file['id'], meta, value))

            archive_time = datetime.utcfromtimestamp(ptr_file['archived time']).replace(tzinfo=timezone.utc).astimezone(tz=None)
            cur.execute("EXECUTE insert_archivefiletoarchivevolume(%d, %d, %d, '%s')" % (volume['id'], file['id'], ptr_file['position'], archive_time.strftime('%F %T%Z')))

            i_file = i_file + 1

    display("commit", True)
    db_conn.commit()
    db_conn.close()
    display(" done")

except:
    print("Catch query error")
    db_conn.rollback()
    raise

