# -*- coding: utf-8 -*-
import thegiant

# this is a quick and dirty Key value server that holds everything in ram
# mainly for test purposes

h = {}

def setgetserver(e, s):
    # print ">>>>>>>>>>>>> here"
    #print e
    # print "=================="
    # print s
    # print "=================="
    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]
        #print h
        return '+OK\r\n'
    elif (e['REDIS_CMD'][0] == 'GET'):
        v = h[e['REDIS_CMD'][1]]
        return '$%s\r\n%s\r\n' % (len(v), v) 
        # return [h[e['REDIS_CMD'][1]]] 

thegiant.run(setgetserver, '0.0.0.0', 6380)
