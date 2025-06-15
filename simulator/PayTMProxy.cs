using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Runtime.ConstrainedExecution;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading;

namespace Simulator
{
    public class PayTMProxy
    {
        //private static readonly string localIP = "10.0.0.20";
        private static readonly int port = 5804;
        private static readonly string machineName = "nos-staging.paytm.com";
        private static readonly string serverName = "nos-staging.paytm.com";
        private static readonly bool isDebugLogs = false;

        static X509Certificate serverCertificate = null;

        public static void RunServer()
        {
            //serverCertificate = X509Certificate.CreateFromCertFile("file.pem");
            //serverCertificate = X509Certificate2.CreateFromPemFile("file.pem", "file.pem");

            //string pass = Guid.NewGuid().ToString();
            //var certificate = X509Certificate2.CreateFromPemFile("Certificate1.cer", "file.pem");
            //serverCertificate = new X509Certificate2(certificate.Export(X509ContentType.Pfx, pass), pass);
            serverCertificate = new X509Certificate2(
                    "Certificate1.pfx",
                    "password",
                    X509KeyStorageFlags.PersistKeySet | X509KeyStorageFlags.MachineKeySet | X509KeyStorageFlags.Exportable
                );

            // Create a TCP/IP (IPv4) socket and listen for incoming connections.
            TcpListener listener = new TcpListener(IPAddress.Parse(Program.LocalIPForAirtel), 8443);
            listener.Start();
            while (true)
            {
                if (isDebugLogs)
                    Console.WriteLine("Waiting for a client to connect to send to host ...");
                TcpClient client = listener.AcceptTcpClient();
                ProcessClient(client);
            }
        }

        static void ProcessClient(TcpClient client)
        {
            // A client has connected. Create the
            // SslStream using the client's network stream.
            SslStream sslStream = new SslStream(
                client.GetStream(), false);
            // Authenticate the server but don't require the client to authenticate.
            try
            {
                sslStream.AuthenticateAsServer(serverCertificate, clientCertificateRequired: false, checkCertificateRevocation: true);
                // Display the properties and settings for the authenticated stream.
                //DisplaySecurityLevel(sslStream);
                //DisplaySecurityServices(sslStream);
                //DisplayCertificateInformation(sslStream);
                //DisplayStreamProperties(sslStream);

                // Set timeouts for the read and write to 5 seconds.
                sslStream.ReadTimeout = 5000 * 100;
                sslStream.WriteTimeout = 5000 * 100;
                // Read a message from the client.
                string messageData = ReadMessage(sslStream);

                Console.WriteLine("Sending message to PayTM .... ");
                Console.WriteLine(messageData);
                var serverMessage = RunClient(messageData);
                //var serverMessage = "";
                Console.WriteLine("Received data from PayTM");
                Console.WriteLine(serverMessage);

                //serverMessage = "HTTP/1.0 200 OK\nContent-Type: application/json;charset=UTF-8\nContent-Length: 458\nx-application-context: application:8080\nAccess-Control-Allow-Origin: *\nAccess-Control-Allow-Methods: GET, POST, OPTIONS, PUT\nAccess-Control-Allow-Headers: DNT, X-CustomHeader, Keep-Alive, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control, Content-Type\nStrict-Transport-Security: max-age=31536000; includeSubDomains;\nExpires: Thu, 16 Mar 2023 14:34:14 GMT\nCache-Control: max-age=0, no-cache, no-store\nPragma: no-cache\nDate: Thu, 16 Mar 2023 14:34:14 GMT\nConnection: close\n\n{\"head\":{\"responseTimestamp\":\"2023-03-0712:44:48\"},\"body\":{\"resultStatus\":\"FAIL\",\"resultCode\":\"F\",\"resultMsg\":\"Request parameters are not valid\",\"resultCodeId\":\"EOS_0002\",\"tid\":null,\"mid\":null,\"offlineSaleBatchResponseList\":null}}";

                byte[] message = Encoding.UTF8.GetBytes(serverMessage);

                if (isDebugLogs)
                {
                    Console.WriteLine("=====================================================");
                    Console.WriteLine("Sending below message to server");
                    Console.WriteLine("Length : " + serverMessage.Length);
                    Console.WriteLine("Byte length : " + message.Length);
                }

                // Write a message to the client.
                sslStream.Write(message);
                //Console.WriteLine("Sent data to device");
                if (isDebugLogs)
                {
                    Console.WriteLine("Message written : " + serverMessage);
                    Console.WriteLine("=====================================================");
                }

            }
            catch (AuthenticationException e)
            {
                Console.WriteLine("Exception: {0}", e.Message);
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
                // The client stream will be closed with the sslStream
                // because we specified this behavior when creating
                // the sslStream.
                sslStream.Close();
                client.Close();
            }
        }

        static string ReadMessage(SslStream sslStream)
        {
            // Read the  message sent by the client.
            // The client signals the end of the message using the
            // "<EOF>" marker.
            byte[] buffer = new byte[2048];
            StringBuilder messageData = new StringBuilder();
            int bytes = -1;
            if (isDebugLogs)
                Console.WriteLine("Going to wait fo reading data");
            do
            {
                // Read the client's test message.
                bytes = sslStream.Read(buffer, 0, buffer.Length);

                // Use Decoder class to convert from bytes to UTF8
                // in case a character spans two buffers.
                Decoder decoder = Encoding.UTF8.GetDecoder();
                char[] chars = new char[decoder.GetCharCount(buffer, 0, bytes)];
                decoder.GetChars(buffer, 0, bytes, chars, 0);
                messageData.Append(chars);
                // Check for EOF or an empty message.
                if (messageData.ToString().IndexOf("<EOF>") != -1)
                {
                    break;
                }
            } while (bytes != 0);

            return messageData.ToString();
        }
        static void DisplaySecurityLevel(SslStream stream)
        {
            if (isDebugLogs)
            {
                Console.WriteLine("Cipher: {0} strength {1}", stream.CipherAlgorithm, stream.CipherStrength);
                Console.WriteLine("Hash: {0} strength {1}", stream.HashAlgorithm, stream.HashStrength);
                Console.WriteLine("Key exchange: {0} strength {1}", stream.KeyExchangeAlgorithm, stream.KeyExchangeStrength);
                Console.WriteLine("Protocol: {0}", stream.SslProtocol);
            }
        }
        static void DisplaySecurityServices(SslStream stream)
        {
            if (isDebugLogs)
            {
                Console.WriteLine("Is authenticated: {0} as server? {1}", stream.IsAuthenticated, stream.IsServer);
                Console.WriteLine("IsSigned: {0}", stream.IsSigned);
                Console.WriteLine("Is Encrypted: {0}", stream.IsEncrypted);
            }
        }
        static void DisplayStreamProperties(SslStream stream)
        {
            if (isDebugLogs)
            {
                Console.WriteLine("Can read: {0}, write {1}", stream.CanRead, stream.CanWrite);
                Console.WriteLine("Can timeout: {0}", stream.CanTimeout);
            }
        }
        static void DisplayCertificateInformation(SslStream stream)
        {
            if (isDebugLogs)
                Console.WriteLine("Certificate revocation list checked: {0}", stream.CheckCertRevocationStatus);

            X509Certificate localCertificate = stream.LocalCertificate;
            if (stream.LocalCertificate != null)
            {
                if (isDebugLogs)
                {
                    Console.WriteLine("Local cert was issued to {0} and is valid from {1} until {2}.",
                    localCertificate.Subject,
                    localCertificate.GetEffectiveDateString(),
                    localCertificate.GetExpirationDateString());
                }
            }
            else
            {
                if (isDebugLogs)
                    Console.WriteLine("Local certificate is null.");
            }
            // Display the properties of the client's certificate.
            X509Certificate remoteCertificate = stream.RemoteCertificate;
            if (stream.RemoteCertificate != null)
            {
                if (isDebugLogs)
                {
                    Console.WriteLine("Remote cert was issued to {0} and is valid from {1} until {2}.",
                    remoteCertificate.Subject,
                    remoteCertificate.GetEffectiveDateString(),
                    remoteCertificate.GetExpirationDateString());
                }
            }
            else
            {
                if (isDebugLogs)
                    Console.WriteLine("Remote certificate is null.");
            }
        }

        public static bool ValidateServerCertificate(
              object sender,
              X509Certificate certificate,
              X509Chain chain,
              SslPolicyErrors sslPolicyErrors)
        {
            if (sslPolicyErrors == SslPolicyErrors.None)
            {
                return true;
            }

            // Do not allow this client to communicate with unauthenticated servers.
            return false;
        }

        public static X509Certificate SelectLocalCertificate(
            object sender,
            string targetHost,
            X509CertificateCollection localCertificates,
            X509Certificate remoteCertificate,
            string[] acceptableIssuers)
        {
            if (acceptableIssuers != null &&
                acceptableIssuers.Length > 0 &&
                localCertificates != null &&
                localCertificates.Count > 0)
            {
                // Use the first certificate that is from an acceptable issuer.
                foreach (X509Certificate certificate in localCertificates)
                {
                    string issuer = certificate.Issuer;
                    if (Array.IndexOf(acceptableIssuers, issuer) != -1)
                    {
                        return certificate;
                    }
                }
            }
            if (localCertificates != null &&
                localCertificates.Count > 0)
            {
                return localCertificates[0];
            }

            return null;
        }

        public static string RunClient(string data)
        {
            // Create a TCP/IP client socket.
            // machineName is the host running the server application.
            TcpClient client = new TcpClient(machineName, 443);
            if (isDebugLogs)
                Console.WriteLine("Client connected.");
            // Create an SSL stream that will close the client's stream.
            SslStream sslStream = new SslStream(
                client.GetStream(),
                false,
                new RemoteCertificateValidationCallback(ValidateServerCertificate),
                new LocalCertificateSelectionCallback(SelectLocalCertificate)
                );
            // The server name must match the name on the server certificate.
            try
            {
                //serverCertificate = X509Certificate2.CreateFromPemFile("file.pem", "file.pem");
                sslStream.AuthenticateAsClient(serverName,
                    new X509CertificateCollection(new X509Certificate[] { (X509Certificate)serverCertificate }),
    SslProtocols.Tls, false);
            }
            catch (AuthenticationException e)
            {
                Console.WriteLine("Exception: {0}", e.Message);
                if (e.InnerException != null)
                {
                    Console.WriteLine("Inner exception: {0}", e.InnerException.Message);
                }
                Console.WriteLine("Authentication failed - closing the connection.");
                client.Close();
                return null;
            }
            // Encode a test message into a byte array.
            // Signal the end of the message using the "<EOF>".

            /*
            string body = "{\n    \"head\": {\n        \"dataKsn\": \"FFFFFFFFFF2345000013\",\n        \"pinBlockKsn\": \"FFFFFFFFFF2345000013\",\n        \"version\": \"1.02.04_20191018D\",\n        \"clientId\": \"0820293517\",\n        \"mac\": \"0B48C28EB8F9CFBB\",\n        \"macKsn\": \"FFFFFFFFFF2345000013\"\n    },\n    \"body\": {\n        \"processingCode\": \"000000\",\n        \"amount\": \"7000\",\n        \"stan\": \"000115\",\n        \"expiryDate\": \"F369BC3E257069B0\",\n        \"time\": \"105429\",\n        \"date\": \"0824\",\n        \"year\": \"2022\",\n        \"invoiceNumber\": \"000019\",\n        \"extendInfo\": {\n            \"CHN\": \"CARD HOLDER\",\n            \"MCPAN\": \"true\",\n            \"macEnable\": \"true\"\n        },\n        \"pinBlock\": \"B2465E4C14C1C361\",\n        \"posConditionCode\": \"00\",\n        \"posEntryMode\": \"051\",\n        \"primaryAccountNr\": \"CEC5581096E3FB56\",\n        \"encryptedTrack2\": \"E61BEAEA418A5A429AB87678774E0843AF88C1766A098071\",\n        \"iccData\": \"9F02060000000001009F03060000000000004F07A0000005241010820219009F3602000F9F0702AB809F2608B86723131790443F9F2701808407A00000052410108E0E000000000000000042031F031E039F34034203009F1E0830303030303132339F0D05A468FC98009F0E0510100000009F0F05A468FC98009F10200FB501A8000000000000000000000000000000000000000000000000000100009F090200649F3303E0F8C89F1A0203569F350122950500800480005F2A0203565F3401009A031911189F4104000002489C01009F3704D86844DE9F3602000F9B02E8005F20144341524420484F4C44455220202020202020202F\",\n        \"tid\": \"14015808\",\n        \"mid\": \"DvFnKU85682298907046\"\n    }\n}";

            data = "POST /eos/serviceCreation HTTP/1.1\r\n";
            data += "Host: pgp-staging.paytm.in\r\n";
            data += "Content-Length: 2\r\n";
            data += "Content-Type: application/json\r\n\r\n";
            data += "{}" + "\r\n";
            data += "Connection: close\r\n";
            */

            //Console.WriteLine("Going to send message : " + data);
            
            byte[] messsage = Encoding.UTF8.GetBytes(data);
            // Send hello message to the server.
            sslStream.Write(messsage);
            sslStream.Flush();
            // Read message from the server.
            string serverMessage = ReadMessageClient(sslStream);
            //Console.WriteLine("Server says: {0}", serverMessage);
            // Close the client connection.
            client.Close();
            return serverMessage;
        }

        static string ReadMessageClient(SslStream sslStream)
        {
            // Read the  message sent by the server.
            // The end of the message is signaled using the
            // "<EOF>" marker.
            byte[] buffer = new byte[2048];
            StringBuilder messageData = new StringBuilder();
            int bytes = -1;
            do
            {
                bytes = sslStream.Read(buffer, 0, buffer.Length);

                // Use Decoder class to convert from bytes to UTF8
                // in case a character spans two buffers.
                Decoder decoder = Encoding.UTF8.GetDecoder();
                char[] chars = new char[decoder.GetCharCount(buffer, 0, bytes)];
                decoder.GetChars(buffer, 0, bytes, chars, 0);
                messageData.Append(chars);
                // Check for EOF.
                if (messageData.ToString().IndexOf("<EOF>") != -1)
                {
                    break;
                }
            } while (bytes != 0);

            return messageData.ToString();
        }

        public static byte[] StringToByteArray(string hex)
        {
            return Enumerable.Range(0, hex.Length)
                             .Where(x => x % 2 == 0)
                             .Select(x => Convert.ToByte(hex.Substring(x, 2), 16))
                             .ToArray();
        }

        public static string ByteToHex(byte[] data)
        {
            return BitConverter.ToString(data).Replace("-", string.Empty);
        }

    }
}

