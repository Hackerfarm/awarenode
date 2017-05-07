from bottle import Bottle, run, static_file, request

import bottle
import bottle_pgsql
import local_config
import json
from datetime import datetime

import os


class DateTimeEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, datetime):
            return o.isoformat()
        return json.JSONEncoder.default(self, o)

bottle.debug(True)
rootdir='/usr/share/www/static/'

app = Bottle()
#plugin = bottle_pgsql.Plugin('dbname=awarenode user={} password={}'.format(local_config.db_user, local_config.db_password))
plugin = bottle_pgsql.Plugin('dbname=awarenode user=postgres password=tototo'.format(local_config.db_user, local_config.db_password))
app.install(plugin)

@app.route('/hello')
def hello():
    return static_file('index.html', root=rootdir)

@app.route('/static/<filepath:path>')
def server_static(filepath):
    return static_file(filepath, root=rootdir)

@app.route('/readings/<sensor_id:int>')
def get_all_readings(sensor_id,db):
    db.execute('SELECT * FROM readings WHERE sensorid=%s;', (sensor_id,))
    rows = db.fetchall()
    return str(json.dumps(rows, cls=DateTimeEncoder))

@app.post('/readings/<sensor_id:int>')
def post_reading(sensor_id, db):
    value = request.forms.value
    tc = request.forms.timestamp
    sl = request.forms.sequence
    if tc == '':
        tc = None
    if sl == '':
        sl = None
    db.execute('INSERT INTO readings (sensorid, value, timestamp_server, sequence_local, timestamp_client) VALUES (%s, %s, now(), %s, %s);', (sensor_id, value, sl, tc))
    return

run(app, host='localhost', port=8080, reloader=True)
