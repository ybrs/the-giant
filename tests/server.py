# -*- coding: utf-8 -*-
from thegiant import server
from thegiant.helpers import OK, reply
# this is a quick and dirty server for test purposes

idletest = 0

def idlecb(v):    
    global idletest
    idletest = 1

h = {}
def app(e):

    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]
        return OK
    elif (e['REDIS_CMD'][0] == 'GET'):
        v = h[e['REDIS_CMD'][1]]
        return reply(v)         
    elif (e['REDIS_CMD'][0] == 'ARRAY'):        
        return ['y', None, '', 'x', 1, 2, 4, 5]
    elif (e['REDIS_CMD'][0] == 'INT'):
        return 123            
    elif (e['REDIS_CMD'][0] == 'GENERATOR'):
        return xrange(1,4)
    elif (e['REDIS_CMD'][0] == 'NULL'):
        return None
    elif (e['REDIS_CMD'][0] == 'IDLECB'):
        # if the 3rd parameter is True, callbacks will be combined, eg, if the callback is already
        # registered in the callback queue, then it will not add the same function again.
        server.defer(idlecb, 0.2, False)        
        return OK
    elif (e['REDIS_CMD'][0] == 'IDLERESULT'):
        return idletest

    raise Exception("unknown command")


if __name__ == '__main__':
    server.run(app, '0.0.0.0', 6380)
