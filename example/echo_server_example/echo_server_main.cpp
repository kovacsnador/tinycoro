#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>

#include <iostream>
#include <vector>
#include <syncstream>

#include <tinycoro/tinycoro_all.h>

using epoll_fd_t = int;
using file_desc_t = int;

template<typename OStreamT = std::basic_ostream<char>>
auto DebugPrint(OStreamT& os = std::cout)
{
    return std::osyncstream{os};
}

int SetNonBlocking(file_desc_t socket)
{
    auto flag = fcntl(socket, F_GETFL, 0);
    if (flag >= 0)
    {
        return fcntl(socket, F_SETFL, flag | O_NONBLOCK);
    }
    return -1;
}

// This is the acceptor coroutine.
// It's intented to accept new client connections on the server socket
// and register the connections into the epoll through events.
tinycoro::Task<void> Acceptor(auto& channel, auto& workersDone)
{
    // notify the waiters if the function exits
    auto onExit = tinycoro::Finally([&workersDone] { 
        // notify that we are done
        workersDone.CountDown();
    });

    std::tuple<file_desc_t, epoll_fd_t> data{};
    while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(data))
    {
        auto[server_fd, epoll_fd] = data;

        // Accept new connection
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        file_desc_t client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);

        if (client_fd == -1)
        {
            // something went wrong
            // simply continue with the next file descriptor
            continue;
        }

        // Set client_fd to non-blocking mode
        if (SetNonBlocking(client_fd) == -1)
        {
            DebugPrint() << "[ERROR] setNonBlocking error for client: " << client_fd << "\n";
            close(client_fd);
            co_return;
        }

        // Register the new client socket with epoll
        epoll_event client_event{};
        client_event.events = EPOLLIN | EPOLLOUT; // ready for read and write
        client_event.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1)
        {
            DebugPrint() << "[ERROR] epoll_ctl error\n";
            close(client_fd);
            co_return;
        }
    }
}

// This coroutine receives and echos back the messages.
// It listens to the channel (tinycoro::BufferedChannel),
// if there is a new entry in the channel that means
// there was an event on the file descriptor and it's ready for read/write. 
tinycoro::Task<void> ReceiveAndEcho(auto& channel, auto& workersDone)
{
    // mutex to pretect write
    static std::mutex writeMtx;

    // notify the waiters if the function exits
    auto onExit = tinycoro::Finally([&workersDone] { 
        // notify that we are done
        workersDone.CountDown();
    });

    file_desc_t client_fd;
    while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(client_fd))
    {
        char buffer[1024];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            if (buffer[0] == 'q' && bytes_read <= 2)
            {
                // close all channels if quit received from any of the clients
                DebugPrint() << "[INFO] Closing the server...\n";
                co_return;
            }

            if (buffer[0] == 'c' && bytes_read <= 2)
            {
                // close this client channel
                close(client_fd);

                // continue, to check if there is data to receive
                continue;
            }

            int32_t writeResult;
            {
                // send the result back to the client
                std::scoped_lock lock{writeMtx};
                writeResult = write(client_fd, buffer, bytes_read);
            }

            // Echo the message back to the client
            if(writeResult < 0)
            {
                // Print if some error accours while write()
                DebugPrint() << "[ERROR] Write error: "  << errno << " for client: " << client_fd << std::endl;
            }
        }
        else
        {
            // Something went wrong,
            // simply continue
            continue;
        }
    }
}

// EPoll coroutine mean to be to handle and poll the events
// from the underlying operation system.
// EPoll pushing his results into 2 channels,
// new connections will be pushed into newConnectionChannel,
// ready file desriptors (read/write) into receiveChannel.
tinycoro::Task<void> EPoll(auto& newConnectionChannel, auto& receiveChannel, file_desc_t server_socket, int32_t max_events, int32_t timeoutMs, auto& workersDone)
{
    int epoll_fd{-1};

    // This callback will be called on function exit
    // Some basic cleanup tasks.
    auto onExit = tinycoro::Finally([&] {
        auto cleanUp = [&]()->tinycoro::Task<> {

            // closing the channels
            newConnectionChannel.Close();
            receiveChannel.Close();

            // wait for the channels to be closed
            co_await workersDone;
            
            // close the epoll_fd
            close(epoll_fd);
        };

        // run cleanup coroutine on the current thread
        // if we leave the coroutine task
        tinycoro::AllOfInline(cleanUp()); 
    });

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        DebugPrint() << "[ERROR] epoll_create1 error\n";
        co_return;
    }

    // Register server_fd with epoll for read events
    epoll_event event{};
    event.events = EPOLLIN; // Ready to read
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        DebugPrint() << "[ERROR] epoll_ctl error\n";
        co_return;
    }

    std::vector<epoll_event> events(max_events);

    // getting the shared stop token
    auto stopToken = co_await tinycoro::this_coro::stop_token();

    DebugPrint() << "[INFO] Start polling on fd: " << epoll_fd << "\n";

    for (;;)
    {
        // Wait for events with a timeout
        auto num_events = epoll_wait(epoll_fd, events.data(), events.size() - 1, timeoutMs);

        if (num_events >= 0)
        {
            // we have some events
            // iterate through and check if there is new connection
            // or we have so data to read from socket
            for (int32_t i = 0; i < num_events; ++i)
            {
                auto sock = events[i].data.fd;

                if (sock == server_socket)
                {
                    // there is a new connection to accept
                    // we push it in the Acceptior channel
                    co_await newConnectionChannel.PushWait(std::make_tuple(sock, epoll_fd));
                }
                else
                {
                    // we have some message to read
                    // Need to notify the receive and echo channel
                    co_await receiveChannel.PushWait(sock);
                }
            }
        }
        else if (num_events == -1)
        {
            // there was an error in epoll_wait
            // we simply return the function
            // in that case the onExit cleanup will be triggered
            DebugPrint() << "[ERROR] epoll_wait error\n";
            co_return;
        }

        
        if(stopToken.stop_requested())
        {
            // checking here the shared stop source
            // In case the stop was requested we return from the corotine task
            DebugPrint() << "[INFO] Stop requested for epoll: " << epoll_fd << "\n";
            co_return;
        }
    }
}

// Simply opens a server socket
file_desc_t CreateServerSocket(uint32_t ip, uint16_t port)
{
    constexpr static file_desc_t s_invalid{-1};

    // create the server socket and get the file descriptor
    auto server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        DebugPrint() << "[ERROR] server_socket not created\n";
        return s_invalid;
    }

    int on = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
    {
        DebugPrint() << "[ERROR] Created server_socket SO_REUSEADDR\n";
        close(server_socket);
        return s_invalid;
    }

    if(SetNonBlocking(server_socket) < 0)
    {
        DebugPrint() << "[ERROR] Created server_socket O_NONBLOCK\n";
        return s_invalid;
    }

    // Address info to bind socket
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = ip;

    char buf[INET_ADDRSTRLEN];

    // Bind socket
    if (bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cout << "[ERROR] Created socket cannot be binded to ( "
                  << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
                  << ":" << ntohs(server_addr.sin_port) << ")\n";
        return s_invalid;
    }

    std::cout << "[INFO] Sock is binded to ("
              << inet_ntop(AF_INET, &server_addr.sin_addr, buf, INET_ADDRSTRLEN)
              << ":" << ntohs(server_addr.sin_port) << ")\n";

    // Start listening
    if (listen(server_socket, SOMAXCONN) < 0)
    {
        DebugPrint() << "[ERROR] Socket cannot be switched to listen mode!\n";
        return s_invalid;
    }

    DebugPrint() << "[INFO] Socket is listening now.\n";

    return server_socket;
}

int main(int argc, const char **argv)
{
    // default port to use
    uint16_t port{12345};

    // default ip
    int32_t ip = INADDR_ANY;

    if(argc >= 2)
    {
        port = std::atoi(argv[1]);
    }

    if(argc >= 3)
    {
        ip = std::atoi(argv[2]);
    }

    auto server_socket = CreateServerSocket(htonl(ip), htons(port));

    if(server_socket < 0)
    {
        // Can't open the server socket
        return -1;
    }

    tinycoro::SoftClock clock;

    // creating a scheduler
    tinycoro::Scheduler scheduler;

    auto socketCleanup = [](auto& socket) { close(socket); };

    // Channel for new connections to be accepted.
    tinycoro::BufferedChannel<std::tuple<file_desc_t, epoll_fd_t>> connToaccept{1000};

    // Channels for receiving and echoing back the messages to the clients.
    tinycoro::BufferedChannel<file_desc_t> receiveAndEcho{10000, socketCleanup};

    // need to match the total count of Acceptor() and ReceiveAndEcho()
    tinycoro::Latch workersDone{10};

    // this is a general timeout
    // if this is over, all other coroutines are cancelled
    auto timeoutTask = tinycoro::SleepForCancellable(clock, 5min);

    // We start all the work here.
    // 2 coroutines making all the pull work from the os.
    // 2 coroutine tasks are responsible to acceppt all the new incomming connections.
    // 8 corouitnes receiving the messages from the clients and echoing back.
    tinycoro::AnyOf(scheduler, EPoll(connToaccept, receiveAndEcho, server_socket, 10000, 100, workersDone),
                               EPoll(connToaccept, receiveAndEcho, server_socket, 10000, 100, workersDone),
                                Acceptor(connToaccept, workersDone),
                                Acceptor(connToaccept, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 ReceiveAndEcho(receiveAndEcho, workersDone),
                                 std::move(timeoutTask));

    close(server_socket);

    return 0;
}