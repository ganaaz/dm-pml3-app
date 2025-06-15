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
	public class ABTProxy
	{
        //private static readonly string localIP = "10.0.0.20";
        private static readonly string machineName = "dev-abt-etpass.datamatics.com";
        private static readonly string serverName = "dev-abt-etpass.datamatics.com";
        private static readonly bool isDebugLogs = true;

        static X509Certificate serverCertificate = null;

        public static void RunServer()
        {
            serverCertificate = new X509Certificate2("cert.pfx", "yourpassword");
            TcpListener listener = new TcpListener(IPAddress.Parse(Program.LocalIPForAirtel), 9443);
            listener.Start();
            while (true)
            {
                if (isDebugLogs)
                    Console.WriteLine("Waiting for a client to connect to send to ABT host ...");
                TcpClient client = listener.AcceptTcpClient();
                ProcessClient(client);
            }
        }

        static void ProcessClient(TcpClient client)
        {
            SslStream sslStream = new SslStream(
                client.GetStream(), false);
            try
            {
                sslStream.AuthenticateAsServer(serverCertificate, clientCertificateRequired: false, checkCertificateRevocation: true);
                sslStream.ReadTimeout = 5000 * 100;
                sslStream.WriteTimeout = 5000 * 100;
                string messageData = ReadMessage(sslStream);
                Console.WriteLine("Sending message to ABT .... ");
                Console.WriteLine(messageData);
                var serverMessage = RunClient(messageData);
                Console.WriteLine("Received data from ABT");
                Console.WriteLine(serverMessage);

                byte[] message = Encoding.UTF8.GetBytes(serverMessage);

                if (isDebugLogs)
                {
                    Console.WriteLine("=====================================================");
                    Console.WriteLine("Sending below message to server");
                    Console.WriteLine("Length : " + serverMessage.Length);
                    Console.WriteLine("Byte length : " + message.Length);
                }

                sslStream.Write(message);
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
                Console.WriteLine("Going to wait for reading data");
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
                break;
                //// Check for EOF or an empty message.
                //if (messageData.ToString().IndexOf("<EOF>") != -1)
                //{
                //    break;
                //}
            } while (bytes != 0);

            return messageData.ToString();
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
    SslProtocols.Tls12, false);
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

