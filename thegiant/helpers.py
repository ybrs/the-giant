
OK = '+OK\r\n'

def reply(v):
	'''
	formats the value as a redis reply
	'''
	return '$%s\r\n%s\r\n' % (len(v), v)