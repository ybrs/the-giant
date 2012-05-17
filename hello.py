# -*- coding: utf-8 -*-
import thegiant
from random import choice

def app1(env, sr):
    sr('200 ok', [('Foo', 'Bar'), ('Blah', 'Blubb'), ('Spam', 'Eggs'), ('Blurg', 'asdasjdaskdasdjj asdk jaks / /a jaksdjkas jkasd jkasdj '),
                  ('asd2easdasdjaksdjdkskjkasdjka', 'oasdjkadk kasdk k k k k k ')])
    return ['hello', 'world']

def app2(env, sr):
    sr('200 ok', [])
    return 'hello'

def app3(env, sr):
    sr('200 abc', [('Content-Length', '12')])
    yield 'Hello'
    yield ' World'
    yield '\n'

h = {}

def app4(e, s):
    # print ">>>>>>>>>>>>> here"
    print e
    # print "=================="
    # print s
    # print "=================="
    if (e['REDIS_CMD'][0] == 'SET'):
        h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]

    # print h
    s('200 ok', [])
    # return '*0\r\n'
    return [None, 'x']

    return ['x', None, 'y']

    return ['foobaryönetmeliği ', 'bar']

    return ['+OK\r\n']

apps = (app1, app2, app3, app4)

def wsgi_app(env, start_response):
    return choice(apps)(env, start_response)

thegiant.run(app4, '0.0.0.0', 6380)
