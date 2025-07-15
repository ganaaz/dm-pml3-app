using System;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

class TcpProxy
{
    private readonly IPAddress _listenIp;
    private readonly int _listenPort;
    private readonly string _remoteIp;
    private readonly int _remotePort;

    public TcpProxy(IPAddress listenIp, int listenPort, string remoteIp, int remotePort)
    {
        _listenIp = listenIp;
        _listenPort = listenPort;
        _remoteIp = remoteIp;
        _remotePort = remotePort;
    }

    public void Start()
    {
        TcpListener listener = new TcpListener(_listenIp, _listenPort);
        listener.Start();
        Console.WriteLine($"Hitachi Proxy started on {_listenIp}:{_listenPort}");

        while (true)
        {
            var client = listener.AcceptTcpClient();
            Console.WriteLine("Client connected.");
            _ = HandleClientAsync(client); // Handle each client concurrently
        }
    }

    private async Task HandleClientAsync(TcpClient client)
    {
        using (client)
        using (var remote = new TcpClient())
        {
            try
            {
                await remote.ConnectAsync(_remoteIp, _remotePort);
                Console.WriteLine($"Connected to remote {_remoteIp}:{_remotePort}");

                // Bi-directional forwarding
                var clientToRemote = ForwardDataAsync(client.GetStream(), remote.GetStream(), "Client -> Remote");
                var remoteToClient = ForwardDataAsync(remote.GetStream(), client.GetStream(), "Remote -> Client");

                await Task.WhenAny(clientToRemote, remoteToClient);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error: " + ex.Message);
            }
        }

        Console.WriteLine("Client disconnected.");
    }

    private async Task ForwardDataAsync(NetworkStream from, NetworkStream to, string direction)
    {
        byte[] buffer = new byte[4096];
        try
        {
            int bytesRead;
            while ((bytesRead = await from.ReadAsync(buffer, 0, buffer.Length)) > 0)
            {
                await to.WriteAsync(buffer, 0, bytesRead);
                await to.FlushAsync();
                Console.WriteLine($"{direction}: {bytesRead} bytes");
                Console.WriteLine("Data : " + ByteToHex(buffer, bytesRead));
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"{direction} error: {ex.Message}");
        }
    }

    private static string ByteToHex(byte[] data, int length)
    {
        return BitConverter.ToString(data.Take(length).ToArray()).Replace("-", string.Empty);
    }

    // static void Main(string[] args)
    // {
    //     // Example usage:
    //     var proxy = new TcpProxy(IPAddress.Parse("127.0.0.1"), 9000, "192.168.1.100", 8080);
    //     proxy.Start();
    // }
}
