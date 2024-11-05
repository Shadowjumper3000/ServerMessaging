# Chat Application

This is a simple chat application implemented in C. It consists of a server and a client that communicate over TCP. The server can handle multiple clients simultaneously using threads.

## Prerequisites

- GCC compiler
- pthread library
- Winsock library (for Windows)

## Getting Started

### Cloning the Repository

```sh
git clone https://github.com/Shadowjumper3000/ServerMessaging.git
cd ServerMessaging
```

### Compiling and Running on Windows

1. **Install MinGW**:
   Download and install MinGW from [MinGW official website](http://www.mingw.org/).

2. **Open Command Prompt**:
   Open Command Prompt and navigate to your project directory.

3. **Compile the Server**:
   ```sh
   gcc -o server sserver.c -lpthread -lws2_32
   ```

4. **Compile the Client**:
   ```sh
   gcc -o client client.c -lpthread -lws2_32
   ```

5. **Run the Server**:
   ```sh
   ./server
   ```

6. **Run the Client**:
   Open another Command Prompt window, navigate to your project directory, and run:
   ```sh
   ./client
   ```

### Compiling and Running on Linux

1. **Install GCC and pthread**:
   Open a terminal and install GCC and pthread if they are not already installed:
   ```sh
   sudo apt update
   sudo apt install build-essential
   ```

2. **Open Terminal**:
   Open a terminal and navigate to your project directory.

3. **Compile the Server**:
   ```sh
   gcc -o server sserver.c -lpthread
   ```

4. **Compile the Client**:
   ```sh
   gcc -o client client.c -lpthread
   ```

5. **Run the Server**:
   ```sh
   ./server
   ```

6. **Run the Client**:
   Open another terminal window, navigate to your project directory, and run:
   ```sh
   ./client
   ```

### Setting Up the Firewall

#### On Linux

1. **Open Ports with UFW**:
   If you are using UFW (Uncomplicated Firewall), you can open the port 8080 for your server:
   ```sh
   sudo ufw allow 8080/tcp
   sudo ufw enable
   ```

2. **Check UFW Status**:
   Verify that the port is open:
   ```sh
   sudo ufw status
   ```

#### On Windows

1. **Open Ports with Windows Firewall**:
   - Open the **Control Panel**.
   - Go to **System and Security** > **Windows Defender Firewall**.
   - Click on **Advanced settings** on the left side.
   - In the **Windows Firewall with Advanced Security** window, click on **Inbound Rules**.
   - Click on **New Rule** on the right side.
   - Select **Port** and click **Next**.
   - Select **TCP** and specify the port number `8080`. Click **Next**.
   - Allow the connection and click **Next**.
   - Choose when the rule applies (Domain, Private, Public) and click **Next**.
   - Give the rule a name and click **Finish**.

### Testing the Server and Client

1. **Start the Server**:
   Follow the steps above to start the server on either Windows or Linux.

2. **Start the Client**:
   Follow the steps above to start the client on either Windows or Linux.

3. **Enter Details**:
   - When prompted, enter your name in the client.
   - Enter the server's IP address when prompted.

4. **Send Messages**:
   - Type messages in the client terminal and press Enter to send them.
   - The server will broadcast the messages to all connected clients.

5. **Exit**:
   - Type `exit` in the client terminal to disconnect from the server.

### Notes

- Ensure that both the server and client are using the same port (`8080` by default).
- The client should connect to the server's IP address.
- The server and client can run on the same machine or different machines within the same network.

### Troubleshooting

1. **Check Server Logs**:
   Ensure your server logs any errors or important events. This can help you diagnose issues.

2. **Verify Server is Running**:
   Ensure the server is running and listening on the correct port:
   ```sh
   netstat -tuln | grep 8080
   ```

3. **Test Connectivity**:
   Use `telnet` or `nc` (netcat) to test connectivity to the server:
   ```sh
   telnet 127.0.0.1 8080
   ```
   or
   ```sh
   nc -zv 127.0.0.1 8080
   ```

4. **Check Client Connection**:
   Ensure the client can connect to the server. If the client fails to connect, check the following:
   - Correct IP address and port.
   - Firewall rules.
   - Server is running.

5. **Debugging with Print Statements**:
   Add print statements in your code to trace the flow and identify where it might be failing.
