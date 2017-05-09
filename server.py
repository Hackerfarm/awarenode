from bottle import Bottle, run, static_file, request, template

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
rootdir='./static/'

app = Bottle()
plugin = bottle_pgsql.Plugin('dbname=awarenode user={} password={}'.format(local_config.db_user, local_config.db_password))
app.install(plugin)

@app.route('/')
@app.route('')
@app.route('/index.html')
@app.route('/hello')
def hello():
    return static_file('index.html', root=rootdir)

@app.route('/static/<filepath:path>')
def server_static(filepath):
    return static_file(filepath, root=rootdir)

@app.route('/readings/<sensor_id:int>')
def get_all_readings(sensor_id,db):
    format=request.query.get('format','json')
    db.execute('SELECT * FROM readings WHERE sensorid=%s;', (sensor_id,))
    rows = db.fetchall()
    if format=='json':
        return str(json.dumps(rows, cls=DateTimeEncoder))
    if format=='table':
        ordering=request.query.get('order_by', 'id')
        return template('readings_table', table=rows, ordering=ordering)
    if format=='graph':
        print 'graph'
        xaxis=request.query.get('order_by', 'id')
        yaxis=request.query.get('show_field', 'value')
        return template('readings_graph', table=rows, xaxis=xaxis, yaxis=yaxis)

@app.post('/new_reading')
def post_reading(db):
    sensor_id = request.forms.sensorid
    value = request.forms.value
    tc = request.forms.timestamp
    sl = request.forms.sequence
    if tc == '':
        tc = None
    if sl == '':
        sl = None
    db.execute('INSERT INTO readings (sensorid, value, timestamp_server, sequence_local, timestamp_client) VALUES (%s, %s, now(), %s, %s);', (sensor_id, value, sl, tc))
    return

run(app, host=local_config.hostname, port=local_config.port, reloader=True)
