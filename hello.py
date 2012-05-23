# -*- coding: utf-8 -*-
import thegiant
from random import choice


h = {}

def app4(e):
    # print ">>>>>>>>>>>>> here"
    print e
    # print "=================="
    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]

    # print h
    
    # return '*0\r\n'
    # return [None, 'x']

    # return ['x', None, 'y']

    # return ['foobaryönetmeliği ', 'bar']
    return xrange(1, 4)
    return [1,2,3]
    # return ['+OK\r\n']


def sometimer():
    print "timer called"
    return 1

class Foo(object):
    def __call__(self):
        print "foo bar...."
    def __del__(self):
        print "!!!! destroyed !!!"

foo = Foo()

a = {'f': foo}

# thegiant.add_timer(1, foo)
# thegiant.add_timer(1, sometimer)

thegiant.run(app4, '0.0.0.0', 6380)
