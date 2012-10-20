'''
TODO: this is not complete, we commit every 5 secs. and until we commit get returns none.
'''
import thegiant
import leveldb

db = leveldb.LevelDB('/tmp/leveldb.db')

def setgetserver(e):
    if (e['REDIS_CMD'][0] == 'SET'):
        db.Put(e['REDIS_CMD'][1], e['REDIS_CMD'][2])
        return '+OK\r\n'
    elif (e['REDIS_CMD'][0] == 'GET'):
        v = db.Get(e['REDIS_CMD'][1])
        return '$%s\r\n%s\r\n' % (len(v), v) 
    raise Exception("unknown command")

thegiant.server.run(setgetserver, '0.0.0.0', 6380)
