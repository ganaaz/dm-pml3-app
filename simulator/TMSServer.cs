using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Net;
using System.Net.Security;
using System.Net.Sockets;
using System.Text;
using Newtonsoft.Json.Linq;
using System.Linq;
using Newtonsoft.Json;

namespace Simulator
{
	public class TMSServer
	{
        public static string DEST_FILE_NAME = "";

		public static void StartTMSServer()
		{
            TcpListener listener = new TcpListener(IPAddress.Parse(Program.LocalIPForAirtel), 49991);
            listener.Start();
            Console.WriteLine("TMS Server listener started on " + Program.LocalIPForAirtel + ", on port 49991");
            while (true)
            {
                TcpClient client = listener.AcceptTcpClient();
                ProcessClient(client);
            }
        }

        static void ProcessClient(TcpClient client)
        {
            string messageData = ReadMessage(client.GetStream(), false);
            Console.WriteLine("Message Read from TMS Server socket");
            Console.WriteLine(ParseJson(messageData));
            /*
            if (!messageData.Contains("\"status\": \"Error\""))
            {
                messageData = ReadMessage(client.GetStream(), true);
                File.Copy(messageData, DEST_FILE_NAME, true);
                Console.WriteLine("File Data Read and saved to : " + TMSServer.DEST_FILE_NAME);
            }
            else
            {
                Console.WriteLine("File not found in server");
            }*/
            //Console.WriteLine(messageData);
        }

        static string ReadMessage(NetworkStream stream, bool isFile)
        {
            List<byte> completeData = new List<byte>();
            int bytes = -1;
            do
            {
                byte[] buffer = new byte[2048];
                bytes = stream.Read(buffer, 0, buffer.Length);

                // Add all data to one list
                for (int i = 0; i < bytes; i++)
                    completeData.Add(buffer[i]);

                // check whether term is received
                int len = completeData.Count;
                if (completeData[len - 1] == 0xFF && completeData[len - 2] == 0xFE
                    && completeData[len - 3] == 0xFD && completeData[len - 4] == 0xFC)
                {
                    Console.WriteLine("Terminating data received");
                    break;
                }

            } while (bytes != 0);

            Console.WriteLine("Total Data Read : " + completeData.Count);
            //for (int i = 0; i < completeData.Count; i++)
            //    Console.Write(completeData[i].ToString("X2") + " ");

            int msgLength = BitConverter.ToInt32(completeData.Take(4).ToArray(), 0);
            Console.WriteLine("Total message length received : " + msgLength);

            byte[] msgArray = new byte[msgLength];
            for (int i = 0; i < msgLength; i++)
                msgArray[i] = completeData[4 + i];

            Decoder decoder = Encoding.UTF8.GetDecoder();
            char[] chars = new char[decoder.GetCharCount(msgArray, 0, msgLength)];
            decoder.GetChars(msgArray, 0, msgLength, chars, 0);
            string message = new string(chars);

            Console.WriteLine("Message Received : ");
            Console.WriteLine(message);

            if (!message.Contains("\"status\": \"Error\"") && message.Contains("DOWNLOAD_FILE"))
            {
                Console.WriteLine("File found in server, reading the data");
                int fileLength = BitConverter.ToInt32(completeData.Skip(4 + msgLength).Take(4).ToArray(), 0);
                Console.WriteLine("File length received : " + fileLength);

                byte[] fileData = completeData.Skip(4 + msgLength + 4).ToArray();
                FileStream fileStream = new FileStream(DEST_FILE_NAME, FileMode.Create);
                fileStream.Write(fileData, 0, fileData.Length);
                fileStream.Close();
                Console.WriteLine("File written to : " + DEST_FILE_NAME);
            }

            if (message.Contains("UPLOAD_FILE") && !message.Contains("\"status\": \"Error\""))
            {
                var msg = JsonConvert.DeserializeObject<SimulatorApp.Message>(message);
                Console.WriteLine("Reader requesting file : " + msg.source);
                Console.WriteLine("Source file requested by reader : " + msg.data.sourceFile);
                Console.WriteLine("Sending the file");

                FileStream fileStream = File.Open(msg.data.sourceFile, FileMode.Open);
                Console.WriteLine("Size of the file : " + fileStream.Length);

                var lengthBytes = BitConverter.GetBytes((int)fileStream.Length);
                Console.WriteLine("Length bytes : " + lengthBytes.Length);

                stream.Write(lengthBytes, 0, lengthBytes.Length);

                byte[] buffer = new byte[512];
                while (true)
                {
                    int read = fileStream.Read(buffer, 0, 512);
                    Console.WriteLine("Data read : " + read);
                    stream.Write(buffer, 0, read);
                    if (read == 0)
                        break;
                }
                Console.WriteLine("All data sent");

                fileStream.Close();
                stream.Close();
            }

            return message;

            /*
            // Read the  message sent by the client.
            // The client signals the end of the message using the
            // "<EOF>" marker.
            byte[] buffer = new byte[2048];
            StringBuilder messageData = new StringBuilder();

            var fileName = Path.GetTempFileName(); //"/Users/ganapathy/Desktop/file";
            FileStream fileStream = new FileStream(fileName, FileMode.Create);

            int bytes = -1;
            do
            {
                // Read the client's test message.
                bytes = stream.Read(buffer, 0, buffer.Length);

                if (isFile)
                {
                    fileStream.Write(buffer, 0, bytes);
                    continue;
                }

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
            fileStream.Close();

            if (isFile)
            {
                Console.WriteLine("File generated at : " + fileName);
                return fileName;
            }

            return messageData.ToString().Replace("<EOF>", "");
            */
        }

        private static string ParseJson(string data)
        {
            try
            {
                return JToken.Parse(data).ToString(Newtonsoft.Json.Formatting.Indented);
            }
            catch (Exception)
            {
                return data;
            }
        }

    }
}

