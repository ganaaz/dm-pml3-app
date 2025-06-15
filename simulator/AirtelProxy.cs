using System;
using System.Linq;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Text;

namespace Simulator
{
    public class AirtelProxy
    {
        private const string ClientHost = "13.203.33.36";
        private static X509Certificate? serverCertificate;

        public static void RunServer()
        {
            serverCertificate = new X509Certificate2("cert.pfx", "yourpassword",
                        X509KeyStorageFlags.PersistKeySet | X509KeyStorageFlags.MachineKeySet
                        | X509KeyStorageFlags.Exportable);

            TcpListener listener = new TcpListener(IPAddress.Parse(Program.LocalIPForAirtel), 8443);
            Console.WriteLine("Starting Airtel Tcp Listener");
            listener.Start();
            while (true)
            {
                TcpClient client = listener.AcceptTcpClient();
                ProcessClient(client);
            }
        }

        static void ProcessClient(TcpClient client)
        {
            SslStream sslStream = new SslStream(client.GetStream(), false);
            try
            {
                if (serverCertificate == null)
                {
                    throw new Exception("Server certificate is null");
                }
                sslStream.AuthenticateAsServer(serverCertificate,
                    clientCertificateRequired: false, checkCertificateRevocation: true);
                sslStream.ReadTimeout = 5000 * 100;
                sslStream.WriteTimeout = 5000 * 100;

                Console.WriteLine("=====================================================");
                string messageData = ReadMessage(sslStream);
                Console.WriteLine("Sending message to AWS Airtel .... ");
                Console.WriteLine("Message : " + messageData);
                string serverResponse = RunClient(messageData);
                Console.WriteLine("Received data from Airtel:");
                Console.WriteLine(serverResponse);

                byte[] responseBytes = Encoding.UTF8.GetBytes(serverResponse);
                Console.WriteLine("Sending below message to server");
                sslStream.Write(responseBytes);
                sslStream.Flush();
                Console.WriteLine("Message written to ssl stream");
                Console.WriteLine("=====================================================");
            }
            catch (AuthenticationException e)
            {
                Console.WriteLine("AuthenticationException: {0}", e.Message);
                if (e.InnerException != null)
                {
                    Console.WriteLine("Inner exception: {0}", e.InnerException.Message);
                }
                Console.WriteLine("Authentication failed - closing the connection.");
                sslStream.Close();
                client.Close();
                return;
            }
            catch (Exception e)
            {
                Console.WriteLine("Other Exception: {0}", e.Message);
                if (e.InnerException != null)
                {
                    Console.WriteLine("Inner exception: {0}", e.InnerException.Message);
                }
                sslStream.Close();
                client.Close();
                return;
            }
            finally
            {
                sslStream.Close();
                client.Close();
            }
        }

        static string RunClient(string data)
        {
            TcpClient client = new TcpClient(ClientHost, 9443);
            SslStream sslStream = new SslStream(client.GetStream(), false,
                new RemoteCertificateValidationCallback(ValidateServerCertificate),
                new LocalCertificateSelectionCallback(SelectLocalCertificate));

            try
            {
                if (serverCertificate == null)
                {
                    throw new Exception("Server certificate is null");
                }
                sslStream.AuthenticateAsClient(ClientHost,
                    new X509CertificateCollection(new X509Certificate[] { serverCertificate }),
                    SslProtocols.Tls12, false);
            }
            catch (AuthenticationException e)
            {
                Console.WriteLine("Authentication failed: " + e.Message);
                client.Close();
            }

            Console.WriteLine("Sending request with replacing host names to client host name");
            string request = data.Replace("Host: localhost:9443", "Host: apbsit.airtelbank.com")
                .Replace("Host: 13.203.33.36:9443", "Host: apbsit.airtelbank.com");
            Console.WriteLine("Request actually sent to Airtel : " + request);
            byte[] message = Encoding.UTF8.GetBytes(request);
            sslStream.Write(message);
            sslStream.Flush();
            return ReadMessage(sslStream);
        }

        static string ReadMessage(SslStream sslStream)
        {
            byte[] buffer = new byte[4096];
            StringBuilder messageData = new StringBuilder();
            int bytesRead;

            while ((bytesRead = sslStream.Read(buffer, 0, buffer.Length)) > 0)
            {
                string part = Encoding.UTF8.GetString(buffer, 0, bytesRead);
                messageData.Append(part);
                if (messageData.ToString().Contains("\r\n\r\n"))
                {
                    break;
                }
            }

            return messageData.ToString();
        }

        public static bool ValidateServerCertificate(
              object sender,
              X509Certificate? certificate,
              X509Chain? chain,
              SslPolicyErrors sslPolicyErrors)
        {
            return true;
        }

        public static X509Certificate SelectLocalCertificate(
            object sender,
            string targetHost,
            X509CertificateCollection localCertificates,
            X509Certificate? remoteCertificate,
            string[] acceptableIssuers)
        {
            if (localCertificates == null || localCertificates.Count == 0)
            {
                throw new ArgumentNullException(nameof(localCertificates));
            }
            return localCertificates[0];
        }
    }
}