# -*- coding: utf-8 -*-
import thegiant

# this is a quick and dirty Key value server that holds everything in ram
# mainly for test purposes

h = {}

def setgetserver(e, s):
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
    elif (e['REDIS_CMD'][0] == 'NULL'):
        return None
    raise Exception("unknown command")


if __name__ == '__main__':
    thegiant.run(setgetserver, '0.0.0.0', 6380)