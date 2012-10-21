from redis import Redis, ResponseError

rediscli = Redis(host='localhost', port=6380)
print rediscli.execute_command("IDLECB")        