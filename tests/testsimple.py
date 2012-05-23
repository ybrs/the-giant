import unittest
import thegiant
import testsimple
from redis import Redis, ResponseError
import multiprocessing
import time
import server

def runserver():
    try:
        thegiant.run(server.setgetserver, '0.0.0.0', 6380)
    except Exception as e:
        print e

class TestProtocol(unittest.TestCase):

    def setUp(self):
        self.p = multiprocessing.Process(target=runserver)
        self.p.start()
        self.rediscli = Redis(host='localhost', port=6380)
        print self.p
        print "sleeping"
        time.sleep(1)

    def testSetGet(self):
        # raw string replies...
        self.rediscli.set('foo', '123')        
        v = self.rediscli.get('foo')
        assert '123' == v
        
        # array replies
        v = self.rediscli.execute_command("ARRAY")
        assert v == ['y', None, '', 'x', 1, 2, 4, 5]

        # integer reply
        v = self.rediscli.execute_command("INT")        
        assert 123 == v

        # null reply
        v = self.rediscli.execute_command("NULL")        
        assert None == v

        # reply with a generator
        v = self.rediscli.execute_command("GENERATOR")        
        assert [1,2,3] == v

        # error
        with self.assertRaises(ResponseError) as error:
            v = self.rediscli.execute_command("UNKNOWN")        
        self.assertEqual(error.exception.message, 'unknown command')

    def tearDown(self):
        self.p.terminate()

if __name__ == '__main__':
    unittest.main()        