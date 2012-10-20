valgrind --suppressions=valgrind-python.supp --log-file=valgrind.log --tool=memcheck --leak-check=full --show-reachable=yes ../python-src/Python-2.7.3/python -E -tt tests/server.py
