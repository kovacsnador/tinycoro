import socket
import concurrent.futures
import time
import sys

SERVER_HOST = "0.0.0.0"  # Replace with your server's address if remote
SERVER_PORT = 12345
NUMBER_OF_CONNECTIONS = 10000  # Total number of concurrent connections
TEST_MESSAGE = "Echo server stress test message!"  # Message to test
CLOSE_MESSAGE = "c"  # Indicate the close the connection
QUIT_MESSAGE = "q"  # Indicate the close the connection
REPEAT_COUNT = 10  # Number of messages per connection
QUIT = False


def echo_client(client_id):
    """Simulate a single client sending and receiving messages."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            client_socket.connect((SERVER_HOST, SERVER_PORT))
            for i in range(REPEAT_COUNT):
                message = f"{TEST_MESSAGE}_{i + 1}"  # Append the counter to the message
                client_socket.sendall(message.encode())
                response = client_socket.recv(1024).decode()
                if response != message:
                    return f"Client {client_id}: Mismatc1h"
            client_socket.sendall(CLOSE_MESSAGE.encode())  # Close the connection
            return f"Client {client_id}: Success"
    except Exception as e:
        return f"Client {client_id}: Error - {e}"


if __name__ == "__main__":

    argc = len(sys.argv)
    if argc > 1:
        NUMBER_OF_CONNECTIONS = int(sys.argv[1])

    if argc > 2:
        QUIT = sys.argv[2].lower() == 'true'

    if argc > 3:
        SERVER_HOST = int(sys.argv[3])

    if argc > 4:
        SERVER_PORT = int(sys.argv[4])


    print(f"Starting stress test on {SERVER_HOST}:{SERVER_PORT} with {NUMBER_OF_CONNECTIONS} connections...")
    start_time = time.time()

    # Use ThreadPoolExecutor for concurrent client connections
    with concurrent.futures.ThreadPoolExecutor(max_workers=NUMBER_OF_CONNECTIONS) as executor:
        # Submit tasks for each client
        futures = [executor.submit(echo_client, i) for i in range(NUMBER_OF_CONNECTIONS)]
        
        # Wait for all tasks to complete and collect results
        results = [future.result() for future in concurrent.futures.as_completed(futures)]

    # Gather statistics
    success_count = sum(1 for result in results if "Success" in result)
    mismatch_count = sum(1 for result in results if "Mismatch" in result)
    error_count = sum(1 for result in results if "Error" in result)

    # Print results
    print("\nDetailed Results:")
    for result in results:
        print(result)

    elapsed_time = time.time() - start_time
    print(f"\nStress test completed in {elapsed_time:.2f} seconds.")
    print(f"\nSummary:")
    print(f"  Total connections: {NUMBER_OF_CONNECTIONS}")
    print(f"  Successful connections: {success_count}")
    print(f"  Mismatches: {mismatch_count}")
    print(f"  Errors: {error_count}")

    if QUIT:
        print("Quit the server...")
        quit_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        quit_socket.connect((SERVER_HOST, SERVER_PORT))
        quit_socket.sendall(QUIT_MESSAGE.encode())
