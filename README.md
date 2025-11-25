# NetThreaded - Multi-threaded HTTP Proxy Server with LRU Cache

A lightweight, multi-threaded HTTP proxy server implementation in C with built-in LRU (Least Recently Used) caching support.

## Features

- Multi-threaded request handling using POSIX threads
- Semaphore-based connection limiting (max 15 concurrent clients)
- LRU cache for frequently accessed content
- Thread-safe cache operations using mutex locks
- Support for HTTP/1.0 and HTTP/1.1 GET requests
- Configurable port number

## Requirements

- GCC/G++ compiler
- Linux/Unix operating system (POSIX-compliant)
- pthread library

## Building

```bash
make
```

## Usage

```bash
# Start proxy server on default port 8080
./proxy

# Start proxy server on custom port
./proxy 3000
```

### Accessing websites through the proxy

Use the following URL format in your browser:

```
http://localhost:8080/http://example.com
```

**Note:** Only HTTP URLs are supported. HTTPS connections are not supported by this proxy.

## Architecture

- **proxy_parse.c/h**: HTTP request parsing library
- **proxy_server_cache.c**: Main proxy server implementation with caching

## Limitations

- Linux/Unix only (uses POSIX APIs)
- Only supports HTTP GET method
- Does not support HTTPS/TLS connections
- Maximum cache element size: 10 KB
- Maximum total cache size: 200 MB

## License

This project is provided as-is for educational purposes.