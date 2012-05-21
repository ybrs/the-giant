# -*- coding: utf-8 -*-
import thegiant
from random import choice


h = {}

def app4(e, s):
    # print ">>>>>>>>>>>>> here"
    #print e
    # print "=================="
    # print s
    # print "=================="
    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]

    # print h
    s('200 ok', [])
    # return '*0\r\n'
    # return [None, 'x']

    # return ['x', None, 'y']

    # return ['foobaryönetmeliği ', 'bar']

    return ['+OK\r\n']


def sometimer(f):
    print "timer called"
    return 1

class Foo(object):
    def sometimer2(self):
        print "timer called"
        return 1
    def __call__(self):
        print "foo"
    def __del__(self):
        print "!!!! destroyed !!!"

foo = Foo()

a = {'f': foo}

thegiant.add_timer(1, foo)
thegiant.add_timer(1, sometimer)

thegiant.run(app4, '0.0.0.0', 6380)
