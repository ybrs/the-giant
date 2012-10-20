the-giant: Redis protocol toolkit - Think it like a wsgi server but speaks redis
====================================================================

Fast and lightweight asynchronous redis server implementation that does nothing. Think it like wsgi server 
that speaks redis. 

Possible use cases
-----------------------------------------
As far as I can think of you can do:

Implement a fast communication channel between backend services:
-----------------------------------------------------------------
You can easily handle thousands of connections with the-giant, and redis protocol is fast and 
the-giant is really fast too. So you can do things in a different way. Eg. you can start 
100 workers in the background, and push your jobs to them and keep the connection 
open, so they immediately push back the results. Of course there are alternative 
ways to do the same, you can do the same with an MQ server, you push your jobs to the broker 
and pull results back, or you can use zeromq.  

Writing a redis proxy - possibly with some intelligence:
------------------------------------------------------------------
Like you ask for a key from the-giant, and your app checks the key in your cache then asks to 
your database if its not in the cache etc.  Or one can implement something like mysql-proxy or 
a load balancer for redis etc...

Writing a redis clone:
------------------------------------------------------------------
Please don't try to do it and just install redis :)

Why ?
----------------------------
For a personal project, I needed to collect data from different servers and persist them to disk. 
I first used ZeroMQ, but its really not very easy to install in different servers - your milage 
may vary - but almost all languages have a working redis client implementation, so it seemed wise 
to write a redis server.

Do I need it ?
-----------------
Probably not. Depending on the task, you might do the same thing with more rock-solid, more 
production tested technologies. Use RabbitMQ, Celery etc. or use a stateless http server etc, or 
use keep-alive with an http server or try using ZeroMQ. 

Why It's Cool
-----------------
* Small, low memory footprint.
* Single-threaded.

Installation
---------------------

libev
-----
Arch Linux
   `pacman -S libev`

Ubuntu
   `apt-get install libev-dev`

Mac OS X (using homebrew_)
   `brew install libev`

Your Contribution Here
   Fork me and send a pull request

the-giant
------
Make sure *libev* is installed and then::

   python setup.py install

Usage
-------------------

    ''' simple key value server '''
    import thegiant
    from thegiant.helpers import OK
    
    h = {}
    def app(e):
        if (e['REDIS_CMD'][0] == 'SET'):
            h[e['REDIS_CMD'][1]] = e['REDIS_CMD'][2]
            return OK
        elif (e['REDIS_CMD'][0] == 'GET'):
            v = h[e['REDIS_CMD'][1]]
            return reply(v)         
    thegiant.server.run(application, '0.0.0.0', 6380)
