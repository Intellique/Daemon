#! /usr/bin/python3

import json
import psycopg2
import sys

if len(sys.argv) < 2:
    print("No job id provide")
    sys.exit(1)

if '..' in sys.argv[1]:
    ids = list(map(lambda x: int(x), sys.argv[1].split('..')))
    job_ids = range(ids[0], ids[1] + 1)
else:
    job_ids = [int(sys.argv[1])]

config_file = '/etc/storiq/storiqone.conf'
if len(sys.argv) > 2:
    config_file = sys.argv[2]

print("Loading configuration (%s)… " % config_file, end="")

try:
    fd_config = open(config_file)
    config = json.load(fd_config)
    fd_config.close()
except:
    print('Failed')
    raise

db_config = {
    'database': config['database'][0]['db'],
    'user': config['database'][0]['user'],
    'password': config['database'][0]['password']
}
if 'host' in config['database'][0]:
    db_config['host'] = config['database'][0]['host']

print("OK\nConnecting to database '%s' with user '%s'… " % (db_config['database'], db_config['user']), end="")

try:
    db_conn = psycopg2.connect(**db_config)
    db_conn.autocommit = False
except:
    print('Failed')
    raise

print("connected")

cur = db_conn.cursor()

for job_id in job_ids:
    print("Looking for job (id: %d)… " % job_id, end="")

    try:
        cur.execute("SELECT name, metadata, login FROM job WHERE id = %s AND type IN (SELECT id FROM jobtype WHERE name = 'create-archive')", (job_id,))
        if cur.rowcount < 1:
            print("Not found")
            continue

        (job_name, metadata_json, login) = cur.fetchone()

        if job_name is not None:
            print("Found, job's name: %s" % job_name)
        else:
            print("Not found")
            continue

        if isinstance(metadata_json, (str,)):
            print("Parsing metadata… ", end="")
            metadata = json.loads(metadata_json)
            print("OK")
        else:
            metadata = metadata_json

        print("Looking for archive… ", end="")
        cur.execute("SELECT DISTINCT archive FROM archivevolume WHERE jobrun IN (SELECT id FROM jobrun WHERE job = %s)", (job_id,))
        archives = cur.fetchall()

        if archives is not None:
            archives = list(map(lambda x: x[0], archives))
            print("Found, archives id: %s" % ', '.join(map(lambda x: str(x), archives)))
            for archive in archives:
                print("Inserting metadata for archive's id %d" % archive)
                for key, value in metadata['archive'].items():
                    print('key: %s, value: %s' % (key, value))
                    cur.execute("INSERT INTO metadata VALUES (%s, 'archive', %s, %s, %s)", (archive, key, json.dumps(value), login))
                print("Metadata for archive's id %d inserted" % archive)
        else:
            print("Not found")

        db_conn.commit()
    except:
        db_conn.rollback()
        print('Failed')

