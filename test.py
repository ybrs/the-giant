from redis import Redis, StrictRedis
r = StrictRedis('localhost', 6380)
r.set('foo', 'bar')

# s = "hello \r\n madafaka \r\n\r\n end"
# r.set('foobar', s)


# s = "1" * (1024 * 64)
# r.set('foobarbaz', s)