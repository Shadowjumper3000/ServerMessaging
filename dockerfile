# Stage 1: Build the application
FROM ubuntu:24.04 AS builder

# Install necessary packages
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the source code
COPY sserver.c socketutil.h ./

# Compile the C program
RUN gcc -o server sserver.c -lpthread

# Stage 2: Create the final lightweight image
FROM ubuntu:24.04

# Install necessary runtime dependencies
RUN apt-get update && apt-get install -y \
    libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the compiled binary from the builder stage
COPY --from=builder /app/server .

# Ensure the binary has execute permissions
RUN chmod +x /app/server

# Expose port 8088
EXPOSE 8088

# Command to run the compiled program
CMD ["./server"]