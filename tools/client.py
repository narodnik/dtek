import zmq

class Client:

    def __init__(self):
        self._context = zmq.Context()
        self._sender_socket = self._context.socket(zmq.PUSH)
        self._sender_socket.connect('tcp://localhost:8888')

        self._receiver_socket = self._context.socket(zmq.SUB)
        self._receiver_socket.connect('tcp://localhost:8889')
        self._receiver_socket.setsockopt(zmq.SUBSCRIBE, b'')

    def send(self, message):
        self._sender_socket.send(message)

    def start(self):
        while True:
            data = self._receiver_socket.recv()
            print(data)

client = Client()
client.send(b'hello')
client.start()

