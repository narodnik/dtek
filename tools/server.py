import zmq

class Server:

    def __init__(self):
        self._context = zmq.Context()

        self._receiver_socket = self._context.socket(zmq.PULL)
        self._receiver_socket.bind('tcp://*:8888')

        self._publish_socket = self._context.socket(zmq.PUB)
        self._publish_socket.bind('tcp://*:8889')

    def start(self):
        while True:
            data = self._receiver_socket.recv()
            print(data)
            self._publish_socket.send(data)

if __name__ == '__main__':
    server = Server()
    server.start()

