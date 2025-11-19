# Connection Troubleshooting Guide

## Problem: Client Cannot Connect to Server (Infinite Connection)

This guide will help you diagnose and fix connection issues between the Zero Ground client and server.

## Diagnostic Steps

### Step 1: Verify Server is Running

1. Start `Zero_Ground.exe` (server)
2. Check the console output for these messages:
   ```
   [INFO] === Starting TCP Server ===
   [INFO] Attempting to bind to port 53000...
   [INFO] Successfully bound to port 53000
   [INFO] Server is now listening for connections on 0.0.0.0:53000
   [INFO] TCP listener thread started
   [INFO] === TCP Listener Thread Started ===
   [INFO] Listening on port 53000 for incoming connections
   ```

3. If you see "Failed to bind to port 53000", the port is already in use:
   - Close any other instances of Zero_Ground.exe
   - Check if another application is using port 53000:
     ```cmd
     netstat -ano | findstr :53000
     ```
   - If a process is using the port, kill it or restart your computer

### Step 2: Verify Server IP Address

1. On the server screen, note the displayed IP address
2. If it shows "127.0.0.1" or "0.0.0.0", the server may not have detected the correct network interface
3. Find your actual IP address:
   ```cmd
   ipconfig
   ```
   Look for "IPv4 Address" under your active network adapter (usually starts with 192.168.x.x or 10.x.x.x)

### Step 3: Test Client Connection

1. Start `Zero_Ground_client.exe` (client)
2. Enter the server IP address (use the actual IP, not 127.0.0.1 if connecting from another machine)
3. Click "CONNECT TO THE SERVER" or press Enter
4. Check the console output for these messages:
   ```
   [INFO] === Starting TCP Handshake ===
   [INFO] Attempting TCP connection to [IP]:53000
   [INFO] TCP socket created, attempting connection...
   [INFO] Connection attempt completed with status: 0
   [INFO] TCP connection established successfully!
   [INFO] Sending ConnectPacket to server...
   [INFO] Send status: 0
   [INFO] ConnectPacket sent successfully
   [INFO] Waiting for MapDataPacket from server...
   ```

### Step 4: Check Server Response

When a client connects, the server should log:
```
[INFO] === New Client Connection Accepted ===
[INFO] Client IP: [client_ip]
[INFO] Waiting for ConnectPacket from client...
[INFO] Receive status: 0, received bytes: 37
[INFO] Valid ConnectPacket received from [client_ip]
[INFO] Player name: Client
[INFO] Sent MapDataPacket header with [N] walls
[INFO] Sent all wall data to client
[INFO] Sent server initial position to client
[INFO] Sent client initial position
[INFO] Client added to connected clients list
```

## Common Issues and Solutions

### Issue 1: Connection Timeout (Status != 0)

**Symptoms**: Client logs show "Connection attempt completed with status: [non-zero]"

**Causes**:
- Server is not running
- Firewall is blocking the connection
- Wrong IP address entered
- Server and client are on different networks

**Solutions**:
1. Verify server is running and listening on port 53000
2. Disable Windows Firewall temporarily to test:
   ```cmd
   netsh advfirewall set allprofiles state off
   ```
   (Remember to re-enable it after testing!)
3. If on the same machine, use "127.0.0.1" as the IP
4. If on different machines, ensure they're on the same network

### Issue 2: Client Hangs on "Waiting for MapDataPacket"

**Symptoms**: Client connects but never receives map data

**Causes**:
- Server accepted connection but failed to send data
- Network packet loss
- Server crashed after accepting connection

**Solutions**:
1. Check server console for error messages
2. Restart both server and client
3. Check server logs for "Sent MapDataPacket header" message
4. If timeout occurs (after 5 seconds), check network stability

### Issue 3: "Failed to bind to port 53000"

**Symptoms**: Server fails to start with bind error

**Causes**:
- Port 53000 is already in use
- Another instance of the server is running
- Insufficient permissions

**Solutions**:
1. Close all instances of Zero_Ground.exe
2. Check for processes using the port:
   ```cmd
   netstat -ano | findstr :53000
   taskkill /PID [process_id] /F
   ```
3. Run as administrator if needed
4. Restart your computer if the port remains locked

### Issue 4: Connection Works but Immediately Disconnects

**Symptoms**: Client connects briefly then shows "Connection lost"

**Causes**:
- UDP port 53001 (server) or 53002 (client) is blocked
- Network instability
- Firewall blocking UDP traffic

**Solutions**:
1. Ensure UDP ports 53001 and 53002 are open
2. Add firewall rules for both TCP and UDP:
   ```cmd
   netsh advfirewall firewall add rule name="Zero Ground Server TCP" dir=in action=allow protocol=TCP localport=53000
   netsh advfirewall firewall add rule name="Zero Ground Server UDP" dir=in action=allow protocol=UDP localport=53001
   netsh advfirewall firewall add rule name="Zero Ground Client UDP" dir=in action=allow protocol=UDP localport=53002
   ```

## Testing on Same Machine (Localhost)

If testing on the same machine:
1. Start server first
2. Use "127.0.0.1" as the IP address in the client
3. Both applications should connect successfully

## Testing on Different Machines (LAN)

If testing on different machines:
1. Ensure both machines are on the same network
2. Find server's IP address using `ipconfig`
3. Use the actual IP address (e.g., 192.168.1.100) in the client
4. Ensure firewall allows connections on both machines

## Debug Mode

To get more detailed logs:
1. Run both applications from command prompt (not by double-clicking)
2. This will keep the console window open to see all log messages
3. Look for [ERROR], [WARNING], and [INFO] messages

## Still Having Issues?

If you've tried all the above and still can't connect:

1. **Collect logs**: Copy all console output from both server and client
2. **Check versions**: Ensure both executables are from the same build
3. **Test network**: Try pinging the server from the client machine:
   ```cmd
   ping [server_ip]
   ```
4. **Antivirus**: Temporarily disable antivirus software to test
5. **VPN**: Disable VPN if active, as it may interfere with local network

## Expected Successful Connection Sequence

**Server Console**:
```
[INFO] === Starting TCP Server ===
[INFO] Successfully bound to port 53000
[INFO] TCP listener thread started
[INFO] === TCP Listener Thread Started ===
[INFO] === New Client Connection Accepted ===
[INFO] Valid ConnectPacket received from [IP]
[INFO] Sent MapDataPacket header with [N] walls
[INFO] Sent all wall data to client
[INFO] Client added to connected clients list
```

**Client Console**:
```
[INFO] === Starting TCP Handshake ===
[INFO] TCP connection established successfully!
[INFO] ConnectPacket sent successfully
[INFO] Waiting for MapDataPacket from server...
[INFO] Receive status: 0, received bytes: [N]
[INFO] MapDataPacket received with [N] walls
[INFO] Received all [N] walls
[INFO] Quadtree built for collision detection
[INFO] TCP handshake completed successfully
```

**Client Screen**: Should show "Connection established" in green with a "READY" button

---

**Note**: All log messages are written to the console. Make sure to run the applications from a command prompt to see these messages!
