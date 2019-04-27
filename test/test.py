import socket
import time
from datetime import datetime
from threading import Thread
import random

num = 1000


def command(sock, cmd, delay, file):
    line = cmd + "\r\n"
    sock.send(line.encode())
    time.sleep(delay)
    data = sock.recv(1024)
    file.write(timestamp() + cmd + "\r\n")
    file.write(timestamp() + data.decode())


def timestamp():
    now = datetime.now()
    time_line = "[" + str(now.hour) + ":" + str(now.minute) + ":" + str(now.second) + ":" + str(now.microsecond) + "]: "
    return time_line


def thread_1(file):
    for i in range(num):
        delay = round(random.randrange(0, 50)/1000, 2)
        sock = socket.socket()
        sock.connect(('localhost', 12345))
        data = sock.recv(1024)
        file.write(timestamp() + data.decode())
        command(sock, "USER tom", delay, file)
        command(sock, "PASS pass_pass_", delay, file)
        command(sock, "STAT", delay, file)
        command(sock, "LIST", delay, file)
        command(sock, "RETR 1", delay, file)
        command(sock, "QUIT", delay, file)
        sock.close()


def thread_2(file):
    for i in range(num):
        delay = round(random.randrange(0, 50)/1000, 2)
        sock = socket.socket()
        sock.connect(('localhost', 12345))
        data = sock.recv(1024)
        now = datetime.now()
        time_line = "[" + str(now.hour) + ":" + str(now.minute) + ":" + str(now.second) + ":" + str(now.microsecond) + "]: "
        file.write(time_line + data.decode())
        command(sock, "USER alex", delay, file)
        command(sock, "PASS admin", delay, file)
        command(sock, "STAT", delay, file)
        command(sock, "LIST", delay, file)
        command(sock, "RETR 1", delay, file)
        command(sock, "QUIT", delay, file)
        sock.close()


if __name__ == "__main__":
    with open('pop3.log', 'a') as file:
        thread1 = Thread(target=thread_1, args=(file,))
        thread2 = Thread(target=thread_2, args=(file,))
        thread1.start()
        thread2.start()
        thread1.join()
        thread2.join()
