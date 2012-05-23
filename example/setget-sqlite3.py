'''
TODO: this is not complete, we commit every 5 secs. and until we commit get returns none.
'''
import thegiant
import sqlite3
conn = sqlite3.connect('/tmp/example.db')

c = conn.cursor()
try:
    c.execute(''' create table kv(key, value)''')
    conn.commit()
except Exception as e:
    print e


def setgetserver(e):
    if (e['REDIS_CMD'][0] == 'SET'):
        c.execute('insert into kv(key, value) values (?, ?)', (e['REDIS_CMD'][1], e['REDIS_CMD'][2]))        
        return '+OK\r\n'
    elif (e['REDIS_CMD'][0] == 'GET'):
        v = h[e['REDIS_CMD'][1]]
        return '$%s\r\n%s\r\n' % (len(v), v) 
    raise Exception("unknown command")


def commit_delayed():
    print "committing"
    conn.commit()
    print "done"

thegiant.add_timer(5, commit_delayed)    
thegiant.run(setgetserver, '0.0.0.0', 6380)
