# TCP Echo Server Stress Test

This Python script performs a stress test on a TCP echo server by simulating a large number of concurrent client connections. Each client sends a series of messages to the server, expects the same message in response, and reports success or failure based on the match.

## Requirements
- Python 3.7+

## Usage

### run the echo server
```bash
# default port 12345 default ip 0.0.0.0
tinycoro_echo_server [port] [ip]
```

### starting stress test
```bash
python echo_server_stress_test.py [NUMBER_OF_CONNECTIONS] [QUIT] [SERVER_HOST] [SERVER_PORT]
```

## Arguments:
- NUMBER_OF_CONNECTIONS: Number of concurrent client connections (default: 10000).
- QUIT: `true` or `false`, whether to send a quit signal to the server after the test (default: false).
- SERVER_HOST: Host address of the server (default: 0.0.0.0).
- SERVER_PORT: Port number of the server (default: 12345).