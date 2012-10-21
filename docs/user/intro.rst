User Guide
=============

Basic steps:
---------------

create a web server that speaks redis:

::

    # -*- coding: utf-8 -*-
    import thegiant
    from thegiant.helpers import OK
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



thats all folks. 

The-Giant parses the request puts it into a variable and passes it to the function you give. More or less like a wsgi server,
the variable you receive is a dictionary and has the following keys.

Currently the only important variables are REDIS_CMD which contains the command and variables of received redis command. 
And REMOTE_ADDR which has the remote clients address.
::
    {'REDIS_CMD': ['SET', 'foo', '123'], 'REMOTE_ADDR': '127.0.0.1'}

Which is an array of variables.


Returning values
------------------------

From the receiving function you can return these type of responses:

**integer**

You can just return an integer from the receiving function, it will be encoded as an integer.


**string**

You can return a string from the receiving function but the-giant cannot know if you are trying to return a raw response
or a string value, thats why there is a helper function reply(). If you are sending a raw response just return it.

eg 

::

    def fn(e):
        return '+OK\r\n'

if you are returning a string value

::

    def fn(e):
        return reply('Foo')

**NULL**

just return None for a null response.

**A generator**

You can return a generator that yields strings or integers from the callback function. For the redis protocol, we need to know
the element count first. So if you are building a generator you need to implement __len__ method besides __iter__

::

    def fn(e):
        return xrange(1,10)

**An array**

You can return an array of integers or strings.

::

    def fn(e):
        return ['Foo', 'Bar', 1, None]

**An OK**

If you don't plan on returning any values but just an ok response, you can use

::

    from thegiant.helpers import OK
    def fn(e):
        return OK

**An error**

Just raise an exception, it will be converted to an error message in redis protocol

::

    def fn(e):
        raise Exception('unknown command')



