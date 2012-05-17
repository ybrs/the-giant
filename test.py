from redis import Redis, StrictRedis
r = StrictRedis('localhost', 6380)
# r.set('foo', 'bar')

# s = "hello \r\n madafaka \r\n\r\n end"
# r.set('foobar', s)

# s = "hello \r\n madafaka \r\n\r\n end"
# r.set('foobar', s)


#s = "1" * (1024 * 64)
# s = "1234567890" * 10
# print "sending ", len(s)
# r.set('foobarbaz' * 2, s)

# r.set('xfoo' * 2, s)

c = r.execute_command('FOO', 'foo', 'bar')
print c