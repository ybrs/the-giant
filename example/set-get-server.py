# -*- coding: utf-8 -*-
import thegiant
from thegiant.helpers import OK
# this is a quick and dirty Key value server that holds everything in ram
# mainly for test purposes

h = {}

def setgetserver(e):
    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]
        return OK
    elif (e['REDIS_CMD'][0] == 'GET'):
        v = h[e['REDIS_CMD'][1]]
        return reply(v)
    raise Exception("unknown command")

thegiant.server.run(setgetserver, '0.0.0.0', 6380)
