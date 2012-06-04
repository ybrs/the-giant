# -*- coding: utf-8 -*-
# this is a to check for garbage collection and memory leaks.
# mainly for test purposes, dont use for anything else.
import thegiant
import gc
import sys

h = {}

def setgetserver(e):
    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]
        return '+OK\r\n'
    elif (e['REDIS_CMD'][0] == 'GET'):
        v = h[e['REDIS_CMD'][1]]
        return '$%s\r\n%s\r\n' % (len(v), v)         
    elif (e['REDIS_CMD'][0] == 'ARRAY'):        
        return ['y', None, '', 'x', 1, 2, 4, 5]
    elif (e['REDIS_CMD'][0] == 'INT'):
        return 123            
    elif (e['REDIS_CMD'][0] == 'GENERATOR'):
        return xrange(1,4)
    elif (e['REDIS_CMD'][0] == 'NULL'):
        return None
    raise Exception("unknown command")


def gc_dump():
    # force collection
    gc.collect()


if __name__ == '__main__':
    gc.enable()
    gc.set_debug(gc.DEBUG_LEAK)

    thegiant.add_timer(1, gc_dump)
    thegiant.run(setgetserver, '0.0.0.0', 6380)
