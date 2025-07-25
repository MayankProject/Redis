# C Redis Implementation

This project is a simplified implementation of Redis, written in C. It functions as a key-value store with additional support for sorted sets.

## Features

*   **Key-Value Store:** Basic GET, SET, and DELETE operations for string keys and values.
*   **Sorted Sets:** Supports adding members with scores to sorted sets (ZADD), retrieving scores (ZSCORE), removing members (ZREM), and ranking members (ZRANK).
*   **In-Memory Storage:** All data is stored in memory.
*   **Networking:** Uses a TCP-based client-server model.
*   **Dynamic Resizing:** The underlying hash table automatically resizes to accommodate a growing number of entries.

## Architecture

The server utilizes a hash map for the key-value store and a combination of a hash map and an AVL tree for the sorted sets. This ensures efficient lookups, insertions, and deletions. The server employs a single-threaded event loop model, utilizing the `poll()` system call to manage multiple client connections concurrently.

*   `server.c`: The main server implementation, handling client connections, request parsing, and command execution.
*   `client.c`: A simple command-line client for interacting with the server.
*   `hashmap.c`/`hashmap.h`: Implementation of the hash map data structure.
*   `avl.c`/`avl.h`: Implementation of the AVL tree for sorted sets.
*   `zset.c`/`zset.h`: Implementation of sorted set operations.
*   `common.h`: Contains common data structures and utility functions.

## Application Flow

1.  **Server Initialization**: The server starts by creating a listening socket on port 8090 and initializes a data structure to manage client connections.

2.  **Event Loop**: The server enters its main event loop, where it uses `poll()` to wait for events on the listening socket and all connected client sockets.

3.  **Client Connection**: When a new client connects, the server accepts the connection, creates a new connection object, and adds the client's socket to the list of monitored file descriptors.

4.  **Request Handling**: When a client sends a request, the server reads the data, parses it into commands and arguments, and executes the appropriate function.

5.  **Data Storage**: For key-value operations, the server uses a hash map. For sorted set operations, it uses a combination of a hash map and an AVL tree to store and retrieve data efficiently.

6.  **Response**: After executing the command, the server sends a response back to the client. The response format is a simple protocol that includes a status code and any requested data.

7.  **Client Disconnection**: If a client disconnects, the server closes the socket and removes it from the list of monitored file descriptors.

### Serialization/Deserialization Flow

The client and server communicate using a custom binary protocol. This protocol defines how commands and data are serialized (converted into a byte stream) and deserialized (converted back into a structured format).

### Client to Server

1.  **Command Assembly**: The client assembles a command as an array of strings (e.g., `{"set", "mykey", "myvalue"}`).

2.  **Serialization**: The client serializes the command into a byte stream with the following format:
    *   **Number of arguments (4 bytes)**: The total number of arguments in the command.
    *   **Argument 1 length (4 bytes)**: The length of the first argument.
    *   **Argument 1 value (variable length)**: The first argument string.
    *   **... (repeat for all arguments)**
3.  **Sending Data**: The client sends the serialized byte stream to the server.

### Server to Client

1.  **Response Assembly**: The server assembles a response, which can be a simple status code, a string, an integer, a double, or an array of other types.
2.  **Serialization**: The server serializes the response into a byte stream with the following format:
    *   **Total response length (4 bytes)**: The total length of the response message.
    *   **Status (4 bytes)**: A response status code (e.g., `RES_OK`, `RES_ERROR`).
    *   **Tag (1 byte)**: A tag indicating the data type of the response (e.g., `TAG_STR`, `TAG_INT`, `TAG_ARR`).
    *   **Data (variable length)**: The actual response data, formatted according to its tag.
3.  **Sending Data**: The server sends the serialized byte stream to the client.

## Getting Started

### Building the Project

To build the server and client, run the following command in the project's root directory:

```bash
make
```

This will generate two executables: `server` and `client`.

### Running the Server

To start the server, execute the following command:

```bash
./server
```

The server will start listening for connections on port 8090.

### Using the Client

To connect to the server, run the client in a separate terminal:

```bash
./client
```

You can then enter commands at the prompt.

## Commands

### Key-Value Commands

*   `set <key> <value>`: Sets the value of a key.
*   `get <key>`: Gets the value of a key.
*   `del <key>`: Deletes a key.
*   `keys`: Returns all keys.

### Sorted Set Commands

*   `zadd <key> <name> <score>`: Adds a member with a score to a sorted set.
*   `zscore <key> <name>`: Gets the score of a member in a sorted set.
*   `zrem <key> <name>`: Removes a member from a sorted set.
*   `zrank <key> <name>`: Gets the rank of a member in a sorted set.
