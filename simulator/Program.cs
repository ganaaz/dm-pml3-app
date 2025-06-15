using System;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Packets;
using MQTTnet.Server;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using SimulatorApp;

namespace Simulator
{
    class Program
    {
        private static DateTime releaseDate = new DateTime(2025, 6, 15);
        private static bool requestUser = false;
        public static string LocalIPForAirtel = "10.0.0.20";
        //public static string LocalIPForPayTM = "192.168.29.248";
        private static string l3Server = "10.0.0.40";
        //private static string l3Server = "192.168.29.248";
        private static int port = 9090;
        private static int dataPort = 9091;
        private static Socket sender;
        private static Socket dataSocket;
        private static bool isConnected = false;
        private static bool isDataConnected = false;
        private static bool isPurchase = false;
        private static string purchaseAmount = "";
        private static string dataFirstByte = "01";
        private static bool sendAmountAtStart = false;
        private static bool isLoopMode = false;
        private static bool autoLoopSingleInReady = false;
        private static bool fetchAuthLoop = false;
        private static int counter = 0;
        private static int searchTimeOut = 20 * 1000; // Milliseconds
        private static bool isSerialConnect;
        private static string serialDataReceived;
        private static string serialComPort = "/dev/tty.usbmodem17feb0701";
        private static bool downloadFileMode = false;
        private static string fileLocalLocation = "";
        private static FileStream downloadFileStream = null;
        private static int purchaseCounter = 0;

        private static SerialPort serialPort;

        static void Main(string[] args)
        {
            Console.WriteLine("-------------------------------------------");
            Console.WriteLine("Datamatics Transit Gate Application for PML3");
            Console.WriteLine("Version : 1.0.0");
            Console.WriteLine($"Release Date : {releaseDate.ToLongDateString()}");
            Console.WriteLine("-------------------------------------------");

            if (requestUser)
            {
                Console.WriteLine("Enter the local IP to run the SSL Server / TMS Server for connecting to board, local network ip : ");
                LocalIPForAirtel = Console.ReadLine();
            }

            // Thread hostThread = new(new ThreadStart(PayTMProxy.RunServer));
            // hostThread.Start();

            // Thread hostThread = new(new ThreadStart(AirtelProxy.RunServer));
            // hostThread.Start();

            // Thread abtThread = new(new ThreadStart(ABTProxy.RunServer));
            // abtThread.Start();

            Console.WriteLine("Do you need to start the TMS Server Listner (Y or N), default N : ");
            var tmsOpt = Console.ReadLine();
            if (!string.IsNullOrWhiteSpace(tmsOpt) && tmsOpt.Equals("Y", StringComparison.OrdinalIgnoreCase))
            {
                Thread tmsThread = new(new ThreadStart(TMSServer.StartTMSServer));
                tmsThread.Start();
                Thread mqttThread = new(new ThreadStart(MqttReceive));
                mqttThread.Start();
            }

            while (true)
            {
                PrintCommands();
                var keyPressed = Console.ReadLine();
                Console.WriteLine();
                bool toStop = false;

                switch (keyPressed)
                {
                    case "q":
                        CloseSocket();
                        toStop = true;
                        break;

                    case "0":
                        if (!isConnected)
                        {
                            ConnectL3();
                        }
                        else
                        {
                            Console.WriteLine("Socket already connected");
                        }
                        break;

                    case "1":
                        if (ValidateConnect())
                        {
                            RequestStatus();
                        }
                        break;

                    case "2":
                        if (ValidateConnect())
                        {
                            isPurchase = false;
                            BalanceInquirySingle();
                        }
                        break;

                    case "3":
                        if (ValidateConnect())
                        {
                            purchaseCounter = 0;
                            isLoopMode = false;
                            Purchase("single");
                        }
                        break;

                    case "4":
                        if (ValidateConnect())
                        {
                            isPurchase = false;
                            BalanceUpdate();
                        }
                        break;

                    case "5":
                        if (ValidateConnect())
                        {
                            ServiceCreation();
                        }
                        break;

                    case "6":
                        if (ValidateConnect())
                        {
                            MoneyAdd();
                        }
                        break;

                    case "7":
                        if (ValidateDataSocketConnect())
                        {
                            FetchAuth("Success");
                        }
                        break;

                    case "8":
                        if (ValidateConnect())
                        {
                            ClearFetchAuth();
                        }
                        break;

                    case "9":
                        if (ValidateConnect())
                        {
                            WriteConfig();
                        }
                        break;

                    case "10":
                        if (ValidateConnect())
                        {
                            FetchAuth("Failure");
                        }
                        break;

                    case "11":
                        if (ValidateConnect())
                        {
                            GetVersion();
                        }
                        break;

                    case "12":
                        if (ValidateConnect())
                        {
                            GetDeviceId();
                        }
                        break;

                    case "13":
                        if (ValidateConnect())
                        {
                            SetReaderTime();
                        }
                        break;

                    case "14":
                        if (ValidateConnect())
                        {
                            isLoopMode = true;
                            Purchase("loop");
                        }
                        break;

                    case "16":
                        if (ValidateConnect())
                        {
                            GetConfig();
                        }
                        break;

                    case "15":
                        if (ValidateConnect())
                        {
                            DoBeep();
                        }
                        break;

                    case "17":
                        if (ValidateConnect())
                        {
                            isLoopMode = false;
                            PurchaseWithDateCheck();
                        }
                        break;

                    case "18":
                        if (ValidateConnect())
                        {
                            isLoopMode = false;
                            VerifyTerminal();
                        }
                        break;

                    case "19":
                        if (ValidateConnect())
                        {
                            AbortPolling();
                        }
                        break;

                    case "20":
                        if (ValidateConnect())
                        {
                            GetPendingOfflineSummary();
                        }
                        break;

                    case "21":
                        if (ValidateConnect())
                        {
                            ChangeIP();
                        }
                        break;

                    case "22":
                        DownloadFile();
                        break;

                    case "23":
                        if (ValidateConnect())
                        {
                            ChangeLogMode();
                        }
                        break;

                    case "24":
                        if (ValidateConnect())
                        {
                            SendConfigAsData();
                        }
                        break;

                    case "25":
                        if (ValidateConnect())
                        {
                            CheckKeyPresent();
                        }
                        break;

                    case "26":
                        if (ValidateConnect())
                        {
                            KeyDestroy();
                        }
                        break;

                    case "27":
                        if (ValidateConnect())
                        {
                            DownloadFileL3Api();
                        }
                        break;

                    case "28":
                        TMSChangeIP();
                        break;

                    case "29":
                        UploadFile();
                        break;

                    case "30":
                        if (ValidateConnect())
                        {
                            GetReversal();
                        }
                        break;

                    case "31":
                        if (ValidateConnect())
                        {
                            FetchAuth("Pending");
                        }
                        break;

                    case "32":
                        if (ValidateConnect())
                        {
                            DeleteFile();
                        }
                        break;

                    case "33":
                        if (ValidateConnect())
                        {
                            ChangeIPOnly();
                        }
                        break;

                    case "34":
                        if (ValidateConnect())
                        {
                            ChangeDNSOnly();
                        }
                        break;

                    case "35":
                        if (ValidateConnect())
                        {
                            GetFirmwareVersion();
                        }
                        break;

                    case "36":
                        if (ValidateConnect())
                        {
                            GetProductOrderNumber();
                        }
                        break;

                    case "37":
                        if (ValidateConnect())
                        {
                            RecalcMac();
                        }
                        break;

                    case "38":
                        if (ValidateConnect())
                        {
                            GetAbtSummary();
                        }
                        break;

                    case "39":
                        if (ValidateConnect())
                        {
                            FetchAbtData();
                        }
                        break;
                    case "40":
                        if (ValidateConnect())
                        {
                            isLoopMode = false;
                            AirtelVerifyTerminal();
                        }
                        break;
                    case "41":
                        if (ValidateConnect())
                        {
                            isLoopMode = false;
                            AirtelHealthCheck();
                        }
                        break;

                    case "98":
                        if (ValidateConnect())
                        {
                            ClearReversal();
                        }
                        break;

                    case "99":
                        if (ValidateConnect())
                        {
                            ServiceBlock();
                        }
                        break;

                    case "101":
                        if (ValidateConnect())
                        {
                            AbortSearch();
                        }
                        break;

                    case "100":
                        if (ValidateConnect())
                        {
                            Reboot();
                        }
                        break;

                    default:
                        Console.WriteLine("Enter a command");
                        break;
                }

                if (toStop)
                {
                    break;
                }
            }

            Console.WriteLine("Simulator process completed");
        }

        private static async Task UploadFile()
        {
            var sourceFile = "/Users/ganapathy/Desktop/update.tar.gz";
            var destFile = "/home/app3/aa/update.tar.gz";

            sourceFile = "/Users/ganapathy/Desktop/file1";
            //destFile = "/home/app3/update/update.spec";

            var deviceId = "17FEB070";

            Console.WriteLine("Uploading file to Reader via TMS");

            if (requestUser)
            {
                Console.WriteLine("Enter the source file to upload : ");
                sourceFile = Console.ReadLine();

                Console.WriteLine("Enter the destination path : ");
                destFile = Console.ReadLine();

                Console.WriteLine("Enter the device Id : ");
                deviceId = Console.ReadLine();
            }

            var message = new Message
            {
                data = new Data
                {
                    destFile = destFile,
                    sourceFile = sourceFile
                },
                messageId = Guid.NewGuid().ToString().Replace("-", ""),
                messageType = "UPLOAD_FILE",
                target = deviceId
            };

            Console.WriteLine("Sending MQTT Message for Upload file");
            await SendMqttMessage(message);
        }

        private static async Task TMSChangeIP()
        {
            var source = "17FEB070";
            Console.WriteLine("Changing the IP Command via TMS");
            if (requestUser)
            {
                Console.WriteLine("Enter the device Id : ");
                source = Console.ReadLine();
            }

            Console.WriteLine("Is the IP is Static (1) or Dynmaic (2), Default (2) : ");
            var opt = Console.ReadLine();

            if (opt == "1")
            {
                var ip = "10.0.0.40";
                var netmask = "255.255.255.0";
                var gateway = "10.0.0.20";
                var dns = "10.0.0.1";

                if (requestUser)
                {
                    // Static
                    Console.WriteLine("Enter the dns name server value, empty if not required : ");
                    dns = Console.ReadLine();

                    Console.WriteLine("Enter the IP Address : ");
                    ip = Console.ReadLine();
                    Console.WriteLine("Enter the Netmask : ");
                    if (string.IsNullOrWhiteSpace(ip))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }

                    netmask = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(netmask))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }

                    Console.WriteLine("Enter the Gateway : ");
                    gateway = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(gateway))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }
                }

                var message = new Message
                {
                    data = new Data
                    {
                        ip_mode = "static",
                        gateway = gateway,
                        ip_address = ip,
                        netmask = netmask
                    },
                    messageId = Guid.NewGuid().ToString().Replace("-", ""),
                    messageType = "CHANGE_IP",
                    target = source
                };

                if (!string.IsNullOrWhiteSpace(dns))
                {
                    message.data.dns = dns;
                }

                Console.WriteLine("Sending MQTT Message for Change ip : Static mode");
                await SendMqttMessage(message);
            }
            else
            {
                // Dynamic
                var dns = "10.0.0.1";

                if (requestUser)
                {
                    Console.WriteLine("Enter the name server value, empty if not required : ");
                    dns = Console.ReadLine();
                }

                var message = new Message
                {
                    data = new Data
                    {
                        ip_mode = "dynamic",
                    },
                    messageId = Guid.NewGuid().ToString().Replace("-", ""),
                    messageType = "CHANGE_IP",
                    target = source
                };

                if (!string.IsNullOrWhiteSpace(dns))
                {
                    message.data.dns = dns;
                }

                Console.WriteLine("Sending MQTT Message for Change ip : Dynamic mode");
                await SendMqttMessage(message);
            }
        }

        private static async Task DownloadFile()
        {
            var file = "/etc/network/interfaces";
            var source = "17FEB070";
            TMSServer.DEST_FILE_NAME = "/Users/ganapathy/Desktop/file1";

            if (requestUser)
            {
                Console.WriteLine("Enter the file name to retrieve eg (/home/app3/l3app.log.0) : ");
                file = Console.ReadLine();
                Console.WriteLine("Enter the device Id : ");
                source = Console.ReadLine();
                Console.WriteLine("Enter the destination file name : ");
                TMSServer.DEST_FILE_NAME = Console.ReadLine();
            }

            var message = new Message
            {
                data = new Data
                {
                    fileName = file
                },
                messageId = Guid.NewGuid().ToString().Replace("-", ""),
                messageType = "DOWNLOAD_FILE",
                target = source
            };

            await SendMqttMessage(message);
        }

        private static async void MqttReceive()
        {
            await ReceiveMqttMessage();
        }

        private static async Task ReceiveMqttMessage()
        {
            var mqttFactory = new MqttFactory();
            using (var mqttClient = mqttFactory.CreateMqttClient())
            {
                var mqttClientOptions = new MqttClientOptionsBuilder().WithTcpServer("localhost").Build();

                // Setup message handling before connecting so that queued messages
                // are also handled properly. When there is no event handler attached all
                // received messages get lost.
                mqttClient.ApplicationMessageReceivedAsync += e =>
                {
                    Console.WriteLine("Received message from MQTT Agent.");
                    Console.WriteLine(ParseJson(e.ApplicationMessage.ConvertPayloadToString()));
                    return Task.CompletedTask;
                };

                await mqttClient.ConnectAsync(mqttClientOptions, CancellationToken.None);

                var mqttSubscribeOptions = mqttFactory.CreateSubscribeOptionsBuilder()
                    .WithTopicFilter(
                        f =>
                        {
                            f.WithTopic("Agent-Server");
                        })
                .Build();

                await mqttClient.SubscribeAsync(mqttSubscribeOptions, CancellationToken.None);

                while(true)
                {

                }
            }
        }

        private static Task MqttClient_ApplicationMessageReceivedAsync(MqttApplicationMessageReceivedEventArgs arg)
        {
            Console.WriteLine("Message Received : " + arg.ApplicationMessage.ConvertPayloadToString());
            return null;
        }

        private static async Task SendMqttMessage(Message message)
        {
            var mqttFactory = new MqttFactory();

            using (var mqttClient = mqttFactory.CreateMqttClient())
            {
                var mqttClientOptions = new MqttClientOptionsBuilder()
                    .WithTcpServer("localhost")
                    .Build();

                await mqttClient.ConnectAsync(mqttClientOptions, CancellationToken.None);

                var applicationMessage = new MqttApplicationMessageBuilder()
                    .WithTopic("Server-Agent")
                    .WithPayload(JsonConvert.SerializeObject(message))
                    .Build();

                await mqttClient.PublishAsync(applicationMessage, CancellationToken.None);

                await mqttClient.DisconnectAsync();

                Console.WriteLine("MQTT application message is published.");
            }
        }

        private static void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            var data = serialPort.ReadExisting();
            data = ParseJson(data);

            if (!string.IsNullOrWhiteSpace(data))
            {
                Console.WriteLine("-----------------------------------------");
                Console.WriteLine($"Serial message received:");
                Console.WriteLine(data);
                Console.WriteLine("-----------------------------------------");
                SendPurchaseAmount(data);

                if (data.Contains("Ready") && autoLoopSingleInReady)
                {
                    Purchase("single");
                }
            }
        }

        private static void PrintCommands()
        {
            Console.WriteLine("");
            Console.WriteLine("Following Commands can be sent to the cVEND Device : ");
            Console.WriteLine("0. Connect to the cVEND device");
            Console.WriteLine("1. Request the status");
            Console.WriteLine("2. Single Balance Inquiry");
            Console.WriteLine("3. Single Purchase Transaction");
            Console.WriteLine("4. Balance Update");
            Console.WriteLine("5. Service Creation");
            Console.WriteLine("6. Money Add");
            Console.WriteLine("98. Clear Reversal");
            Console.WriteLine("7. Fetch  Auth");
            Console.WriteLine("8. Clear Fetch Auth");
            Console.WriteLine("9. Send Config");
            Console.WriteLine("10. Fetch  Auth Failure");
            Console.WriteLine("11. Get Version");
            Console.WriteLine("12. Get Device Id");
            Console.WriteLine("13. Set Time");
            Console.WriteLine("14. Purchase Loop Transaction");
            Console.WriteLine("15. Do Beep");
            Console.WriteLine("16. Get Config");
            Console.WriteLine("17. Purchase with Date Check");
            Console.WriteLine("18. Verify Terminal");
            Console.WriteLine("19. Abort Card Polling");
            Console.WriteLine("20. Get Pending Offline Summary");
            Console.WriteLine("21. Change IP");
            Console.WriteLine("22. Request Download File via TMS");
            Console.WriteLine("23. Change Log Mode");
            Console.WriteLine("24. Send config as data");
            Console.WriteLine("25. Check is key present");
            Console.WriteLine("26. Destroy Key");
            Console.WriteLine("27. Download File via L3 API");
            Console.WriteLine("28. TMS Change IP");
            Console.WriteLine("29. Upload File via TMS");
            Console.WriteLine("30. Get Reversal");
            Console.WriteLine("31. Fetch Auth Pending");
            Console.WriteLine("32. Delete File");
            Console.WriteLine("33. Change Only IP");
            Console.WriteLine("34. Change Only DNS");
            Console.WriteLine("35. Get Firmware Version");
            Console.WriteLine("36. Get Product Order Number");
            Console.WriteLine("37. Recalc mac");
            Console.WriteLine("38. ABT Summary");
            Console.WriteLine("39. ABT Fetch Data");
            Console.WriteLine("40. Verify Airtel Terminal");
            Console.WriteLine("41. Airtel Health Check");
            Console.WriteLine("101. Stop Search");
            Console.WriteLine("100. Reboot");
            Console.WriteLine("Q. Quit");
            Console.WriteLine("");
            Console.WriteLine("Enter the command key : ");
        }

        private static bool ValidateConnect()
        {
            if (!isConnected)
            {
                Console.WriteLine("Please connect to the server first");
                return false;
            }

            return true;
        }

        private static bool ValidateDataSocketConnect()
        {
            if (!isDataConnected)
            {
                Console.WriteLine("Please connect to the data server first");
                return false;
            }

            return true;
        }

        private static void ConnectL3()
        {
            try
            {
                isConnected = false;
                Console.WriteLine("Do you need to connect with Serial port (1) or Ethernet (2). Enter either 1 or 2, default 2 : ");
                var connectOpt = Console.ReadLine();

                if (connectOpt == "1")
                {
                    isSerialConnect = true;
                    if (requestUser)
                    {
                        Console.WriteLine("Enter the com port name where the USB is connected eg COM3 : ");
                        serialComPort = Console.ReadLine();
                    }
                }
                else
                {
                    if (requestUser)
                    {
                        Console.WriteLine("Enter the IP of the L3 Server to Connect  : ");
                        l3Server = Console.ReadLine();
                    }
                }

                if (isSerialConnect)
                {
                    serialPort = new SerialPort(serialComPort, 57600, Parity.None, 8, StopBits.One);
                    serialPort.DtrEnable = true;
                    serialPort.DataReceived += SerialPort_DataReceived;
                    serialPort.Open();
                    isSerialConnect = true;
                    isConnected = true;
                    isDataConnected = true;
                    return;
                }

                IPAddress ipAddress = IPAddress.Parse(l3Server);
                IPEndPoint remoteEP = new IPEndPoint(ipAddress, port);

                // Create a TCP/IP  socket.    
                sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                sender.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
                sender.Connect(remoteEP);

                Console.WriteLine("Socket connected to {0}", sender.RemoteEndPoint.ToString());
                isConnected = true;

                Thread thread = new(new ThreadStart(ReceiveMessage));
                thread.Start();

                IPEndPoint remoteDataEP = new IPEndPoint(ipAddress, dataPort);

                // Create a TCP/IP  socket.    
                dataSocket = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                dataSocket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
                dataSocket.ReceiveBufferSize = 1024 * 1000;
                dataSocket.Connect(remoteDataEP);

                Console.WriteLine("Data Socket connected to {0}", dataSocket.RemoteEndPoint.ToString());
                isConnected = true;

                Thread dataThread = new(new ThreadStart(ReceiveDataSocketMessage));
                dataThread.Start();
                isDataConnected = true;

            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in connecting to L3 : {ex}");
            }
        }

        private static void ChangeIP()
        {
            Console.WriteLine("Changing the IP Command");
            var dns = ""; // "10.0.0.1";
            var dns2 = ""; // "10.0.0.2";
            var dns3 = ""; // "10.0.0.3";
            var dns4 = ""; // "10.0.0.4";
            var searchDomain = "local";

            if (requestUser)
            {
                Console.WriteLine("Enter the dns name server value, empty if not required : ");
                dns = Console.ReadLine();

                Console.WriteLine("Enter the dns2 name server value, empty if not required : ");
                dns2 = Console.ReadLine();

                Console.WriteLine("Enter the dns3 name server value, empty if not required : ");
                dns3 = Console.ReadLine();

                Console.WriteLine("Enter the dns4 name server value, empty if not required : ");
                dns4 = Console.ReadLine();

                Console.WriteLine("Enter the search domain value, empty if not required : ");
                searchDomain = Console.ReadLine();
            }


            Console.WriteLine("Is the IP is Static (1) or Dynmaic (2), Default (2) : ");
            var opt = Console.ReadLine();

            if (opt == "1")
            {
                var ip = "10.0.0.40";
                var netmask = "255.255.255.0";
                var gateway = "10.0.0.20";

                if (requestUser)
                {
                    // Static
                    Console.WriteLine("Enter the IP Address : ");
                    ip = Console.ReadLine();
                    Console.WriteLine("Enter the Netmask : ");
                    if (string.IsNullOrWhiteSpace(ip))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }

                    netmask = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(netmask))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }

                    Console.WriteLine("Enter the Gateway : ");
                    gateway = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(gateway))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }
                }

                var command = new Command
                {
                    Cmd = "change_ip",
                    IpMode = "static",
                    IpAddress = ip,
                    Netmask = netmask,
                    Gateway = gateway
                };
                if (!string.IsNullOrWhiteSpace(dns))
                {
                    command.Dns = dns;
                }
                if (!string.IsNullOrWhiteSpace(dns2))
                {
                    command.Dns2 = dns2;
                }
                if (!string.IsNullOrWhiteSpace(dns3))
                {
                    command.Dns3 = dns3;
                }
                if (!string.IsNullOrWhiteSpace(dns4))
                {
                    command.Dns4 = dns4;
                }
                if (!string.IsNullOrWhiteSpace(searchDomain))
                {
                    command.SearchDomain = searchDomain;
                }
                var strData = JsonConvert.SerializeObject(command);
                var result = SendMessage(strData);
            }
            else
            {
                // Dynamic
                if (requestUser)
                {
                    Console.WriteLine("Enter the name server value, empty if not required : ");
                    dns = Console.ReadLine();
                }

                var command = new Command
                {
                    Cmd = "change_ip",
                    IpMode = "dynamic"
                };
                if (!string.IsNullOrWhiteSpace(dns))
                {
                    command.Dns = dns;
                }
                if (!string.IsNullOrWhiteSpace(dns2))
                {
                    command.Dns2 = dns2;
                }
                if (!string.IsNullOrWhiteSpace(dns3))
                {
                    command.Dns3 = dns3;
                }
                if (!string.IsNullOrWhiteSpace(dns4))
                {
                    command.Dns4 = dns4;
                }
                if (!string.IsNullOrWhiteSpace(searchDomain))
                {
                    command.SearchDomain = searchDomain;
                }
                var strData = JsonConvert.SerializeObject(command);
                var result = SendMessage(strData);
            }
        }

        private static void ChangeDNSOnly()
        {
            Console.WriteLine("Changing the DNS Only Command");
            var dns = "10.0.0.1";
            var dns2 = ""; // "10.0.0.2";
            var dns3 = ""; // "10.0.0.3";
            var dns4 = ""; // "10.0.0.4";
            var searchDomain = "local";

            if (requestUser)
            {
                Console.WriteLine("Enter the dns name server value, empty if not required : ");
                dns = Console.ReadLine();

                Console.WriteLine("Enter the dns2 name server value, empty if not required : ");
                dns2 = Console.ReadLine();

                Console.WriteLine("Enter the dns3 name server value, empty if not required : ");
                dns3 = Console.ReadLine();

                Console.WriteLine("Enter the dns4 name server value, empty if not required : ");
                dns4 = Console.ReadLine();

                Console.WriteLine("Enter the search domain value, empty if not required : ");
                searchDomain = Console.ReadLine();
            }

            var command = new Command
            {
                Cmd = "change_only_dns",
            };
            if (!string.IsNullOrWhiteSpace(dns))
            {
                command.Dns = dns;
            }
            if (!string.IsNullOrWhiteSpace(dns2))
            {
                command.Dns2 = dns2;
            }
            if (!string.IsNullOrWhiteSpace(dns3))
            {
                command.Dns3 = dns3;
            }
            if (!string.IsNullOrWhiteSpace(dns4))
            {
                command.Dns4 = dns4;
            }
            if (!string.IsNullOrWhiteSpace(searchDomain))
            {
                command.SearchDomain = searchDomain;
            }
            var strData = JsonConvert.SerializeObject(command);
            var result = SendMessage(strData);
        }

        private static void ChangeIPOnly()
        {
            Console.WriteLine("Changing the IP Only Command");

            Console.WriteLine("Is the IP is Static (1) or Dynmaic (2), Default (2) : ");
            var opt = Console.ReadLine();

            if (opt == "1")
            {
                var ip = "10.0.0.40";
                var netmask = "255.255.255.0";
                var gateway = "10.0.0.20";

                if (requestUser)
                {
                    // Static
                    Console.WriteLine("Enter the IP Address : ");
                    ip = Console.ReadLine();
                    Console.WriteLine("Enter the Netmask : ");
                    if (string.IsNullOrWhiteSpace(ip))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }

                    netmask = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(netmask))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }

                    Console.WriteLine("Enter the Gateway : ");
                    gateway = Console.ReadLine();
                    if (string.IsNullOrWhiteSpace(gateway))
                    {
                        Console.WriteLine("Empty data. Exit");
                        return;
                    }
                }

                var command = new Command
                {
                    Cmd = "change_only_ip",
                    IpMode = "static",
                    IpAddress = ip,
                    Netmask = netmask,
                    Gateway = gateway
                };
                var strData = JsonConvert.SerializeObject(command);
                var result = SendMessage(strData);
            }
            else
            {
                // Dynamic
                var command = new Command
                {
                    Cmd = "change_only_ip",
                    IpMode = "dynamic"
                };
                var strData = JsonConvert.SerializeObject(command);
                var result = SendMessage(strData);
            }
        }

        private static void ChangeLogMode()
        {
            Console.WriteLine("Changing the Log mode command");
            Console.WriteLine("Enter 1 for Error and 2 for Debug (Default is 1) : ");
            var data = Console.ReadLine();
            var logMode = "Error";
            if (data == "2")
                logMode = "Debug";

            var command = new Command
            {
                Cmd = "change_log_mode",
                LogMode = logMode

            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void SendConfigAsData()
        {
            Console.WriteLine("Sending config as data to L3");
            Console.WriteLine("Enter the path of the file which contains config data : ");
            var fileName = Console.ReadLine();

            if (string.IsNullOrWhiteSpace(fileName))
            {
                Console.WriteLine("Empty input");
                return;
            }

            if (!File.Exists(fileName))
            {
                Console.WriteLine("File : " + fileName + " does not exists");
                return;
            }

            var data = File.ReadAllText(fileName);
            Console.WriteLine("File read and going to send the following data");
            Console.WriteLine(data);
            SendMessage(data);
        }

        private static void CloseSocket()
        {
            try
            {
                if (sender != null && sender.Connected && isConnected)
                {
                    sender.Shutdown(SocketShutdown.Both);
                    sender.Close();
                    Console.WriteLine("Socket closed successfully");
                }

                if (dataSocket != null && dataSocket.Connected && isDataConnected)
                {
                    dataSocket.Shutdown(SocketShutdown.Both);
                    dataSocket.Close();
                    Console.WriteLine("Data Socket closed successfully");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error occurred in closing : {ex}");
            }
        }

        private static void BalanceInquirySingle()
        {
            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "balance_inquiry",
                TimeOut = 20 * 1000
            };
            var strData = JsonConvert.SerializeObject(command);
            var result = SendMessage(strData);

            Console.WriteLine($"Result : {result}");

        }

        private static void GetConfig()
        {
            var command = new Command
            {
                Cmd = "get_config"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void GetVersion()
        {
            var command = new Command
            {
                Cmd = "get_version"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void GetFirmwareVersion()
        {
            var command = new Command
            {
                Cmd = "get_firmware_version"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void GetProductOrderNumber()
        {
            var command = new Command
            {
                Cmd = "get_product_order_number"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void RecalcMac()
        {
            var command = new Command
            {
                Cmd = "recalc_mac"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void GetDeviceId()
        {
            var command = new Command
            {
                Cmd = "get_device_id"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void GetPendingOfflineSummary()
        {
            var command = new Command
            {
                Cmd = "get_pending_offline_summary"
            };

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void GetAbtSummary()
        {
            var command = new Command
            {
                Cmd = "abt_summary"
            };

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void DownloadFileL3Api()
        {
            downloadFileStream = null;
            downloadFileMode = true;
            fileLocalLocation = "";
            Console.WriteLine("Enter the file name to download : ");
            var fileName = "/home/app3/logaaa.txt";

            if (requestUser)
            {
                fileName = Console.ReadLine();
            }

            if (string.IsNullOrWhiteSpace(fileName))
            {
                Console.WriteLine("Provide valid name");
                return;
            }

            Console.WriteLine("Enter the local location to save the file : ");
            var fileSave = "/Users/ganapathy/a.txt";

            if (requestUser)
            {
                fileSave = Console.ReadLine();
            }

            if (string.IsNullOrWhiteSpace(fileSave))
            {
                Console.WriteLine("Cannot be empty");
                return;
            }

            if (File.Exists(fileSave))
            {
                Console.WriteLine("File already exists, please provide a different file.");
                return;
            }

            fileLocalLocation = fileSave;
            
            var command = new Command
            {
                Cmd = "download_file",
                DownloadFileName = fileName
            };

            downloadFileStream = new FileStream(fileLocalLocation, FileMode.Create);
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void KeyDestroy()
        {
            var command = new Command
            {
                Cmd = "destroy_key"
            };

            Console.WriteLine("Enter the label to destroy the key: ");
            string label = Console.ReadLine();
            Console.WriteLine("Enter 1 if its BDK, 2 if its Rupay key : ");
            string type = Console.ReadLine();

            command.Label = label;
            if (type == "1")
                command.IsBdk = true;
            else
                command.IsBdk = false;

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void CheckKeyPresent()
        {
            var command = new Command
            {
                Cmd = "is_key_present"
            };

            Console.WriteLine("Enter the label : ");
            string label = Console.ReadLine();
            Console.WriteLine("Enter 1 if its BDK, 2 if its Rupay key : ");
            string type = Console.ReadLine();

            command.Label = label;
            if (type == "1")
                command.IsBdk = true;
            else
                command.IsBdk = false;

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void WriteConfig()
        {
            var command = new Command
            {
                Cmd = "set_config",
                config = new Config
                {
                    general = new General
                    {
                        nsec_awaiting_write = 99,
                        kldIp = "1.1.1.1",
                        keyInjectRetryDelaySec = 11,
                        maxKeyInjectionTry = 22,
                        panTokenKey = "PAN",
                        beepOnCardFound = false,
                        purchaseLimit = "1000",
                        moneyAddLimit = "1100",
                        deviceCode = "D1",
                        equipmentType = "ET",
                        equipmentCode = "EC",
                        stationId = "SI",
                        stationName = "SN",
                        paytmLogCount = 11,
                        paytmMaxLogSizeKb = 12
                    },
                    psp = new Psp
                    {
                        hostProcessTimeInMinutes = 100,
                        reversalTimeInMinutes = 11,
                        ip = "1.7.140.58",
                        maxRetry = 10,
                        merchantId = "ING000000000006",
                        clientId = "FIECP012310",
                        clientName = "ABC",
                        port = 5804,
                        terminalId = "ING00021",
                        hostVersion = "1.0",
                        txnTimeOut = 90,
                        httpsHostName = "eos-staging.paytm.com",
                        offlineUrl = "offlineUrl",
                        balanceUpdateUrl = "balanceUpdateUrl",
                        moneyLoadUrl = "moneyLoadUrl",
                        serviceCreationUrl = "serviceCreationUrl",
                        verifyTerminalUrl = "verifyTerminalUrl",
                        reversalUrl = "reversalUrl"
                    },
                    keys = new System.Collections.Generic.List<KeyData>
                    {
                        new KeyData
                        {
                            label = "RUPAY_PRM_ACQ_FF0101",
                            slot = 4,
                            mkVersion = 0,
                            astId = "00A",
                            pkcsId = "0015",
                            type = "RUPAY_SERVICE"
                        },
                        new KeyData
                        {
                            label = "RUPAY_PRM_ACQ_601201",
                            slot = 4,
                            mkVersion = 0,
                            astId = "00A",
                            pkcsId = "0016",
                            type = "RUPAY_SERVICE"
                        },
                        new KeyData
                        {
                            label = "BDK_00000000011",
                            slot = 4,
                            mkVersion = 5,
                            astId = "00A",
                            pkcsId = "0005",
                            keySetIdentifier = "0000000001",
                            type = "DUKPT_KEY",
                            isMac = true
                        },
                        new KeyData
                        {
                            label = "BDK_00000000012",
                            slot = 4,
                            mkVersion = 5,
                            astId = "00A",
                            pkcsId = "0005",
                            keySetIdentifier = "0000000001",
                            type = "DUKPT_KEY",
                            isMac = false
                        }
                    },
                    ledConfigs = new System.Collections.Generic.List<object>
                    {
                        new L1
                        {
                            waiting_key_injection = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L2
                        {
                            key_inject = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L3
                        {
                            awating_card_success = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L4
                        {
                            awaiting_card_failure = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L5
                        {
                            card_read_failed = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L6
                        {
                            card_presented = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L7
                        {
                            write_success = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L8
                        {
                            write_failed = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L9
                        {
                            card_processed_success = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L10
                        {
                            card_processed_failure = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L11
                        {
                            card_processed_success_msg_sent = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L12
                        {
                            app_started = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                        new L13
                        {
                            app_exiting = new System.Collections.Generic.List<string> { "N", "N", "N", "N", "R" }
                        },
                    }
                }                
            };
            
            var strData = JsonConvert.SerializeObject(command);
            Console.WriteLine("Data to send : " + strData);
            SendMessage(strData);
        }

        private static void FetchAuth(string mode)
        {
            if (fetchAuthLoop)
            {
                var command1 = new Command
                {
                    Cmd = "fetch_auth",
                    Mode = mode,
                    FetchId = 1,
                    MaxRecords = 2
                };
                var data = JsonConvert.SerializeObject(command1);
                SendDataSocketMessage(data);
                return;
            }

            Console.Write("Enter the Fetch ID : ");
            var id = Console.ReadLine();
            Console.Write("Enter max records : ");
            var records = Console.ReadLine();
            var command = new Command
            {
                Cmd = "fetch_auth",
                Mode = mode,
                FetchId = int.Parse(id),
                MaxRecords = int.Parse(records)
            };
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void FetchAbtData()
        {
            Console.WriteLine("Enter the mode to fetch (Pending, OK, NOK) : ");
            string mode = Console.ReadLine();
            Console.WriteLine("Enter max records : ");
            var records = Console.ReadLine();

            Console.WriteLine("Enter records to skip: ");
            var skipRecords = Console.ReadLine();

            var command = new Command
            {
                Cmd = "abt_fetch",
                Mode = mode,
                MaxRecords = int.Parse(records),
                SkipRecords = int.Parse(skipRecords)
            };
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void ClearFetchAuth()
        {
            Console.Write("Enter the Fetch ID : ");
            var id = Console.ReadLine();
            var command = new Command
            {
                Cmd = "fetch_ack",
                FetchId = int.Parse(id)
            };
            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void BalanceUpdate()
        {
            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "balance_update",
                TimeOut = 10 * 1000
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void MoneyAdd()
        {
            Console.WriteLine("Enter the amount for Money Add");
            Console.WriteLine("For example if the amount is 110.00, Please enter as 11000. 0 not allowed");
            Console.Write("Amount : ");
            var moneyAddAmount = Console.ReadLine();
            //purchaseAmount = "0";
            Console.WriteLine("Money Add Amount : " + moneyAddAmount);

            int.TryParse(moneyAddAmount, out var result);
            if (result == 0)
            {
                Console.WriteLine("Wrong amount entered");
                return;
            }

            Console.WriteLine("Enter the Type, 1 - CC, 2 - DC, 3 - UPI, 4 - CASH, 5 - ACCOUNT");
            Console.Write("Type : ");
            var addType = Console.ReadLine();
            var addTypeStr = "";

            if (addType == "1")
                addTypeStr = "CC";

            if (addType == "2")
                addTypeStr = "DC";

            if (addType == "2")
                addTypeStr = "UPI";

            if (addType == "4")
                addTypeStr = "CASH";

            if (addType == "5")
                addTypeStr = "ACCOUNT";

            if (addTypeStr == "")
            {
                Console.WriteLine("Invalid type. ");
                return;
            }
            Console.WriteLine("Money Add Type : " + addTypeStr);

            Console.WriteLine("Enter the Source Txn Id for Money Add : ");
            var sourceTxnId = Console.ReadLine();
            if (sourceTxnId.Trim() == "")
            {
                Console.WriteLine("Invalid sourceTxnId. ");
                return;
            }
            Console.WriteLine("sourceTxnId : " + sourceTxnId);


            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "money_add",
                TimeOut = 10 * 1000,
                Amount = moneyAddAmount,
                MoneyAddType = addTypeStr,
                MoneyAddSourceTxn = sourceTxnId
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void ServiceCreation()
        {
            Console.WriteLine("Enter 4 digit service id");
            var serviceId = Console.ReadLine();

            if (string.IsNullOrWhiteSpace(serviceId))
            {
                Console.WriteLine("Invalid input");
                return;
            }

            if (serviceId.Length != 4)
            {
                Console.WriteLine("Service id to be of length only 4");
                return;
            }

            if (!OnlyHexInString(serviceId))
            {
                Console.WriteLine("Service id can only be hexa decimal");
                return;
            }    

            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "service_create",
                TimeOut = 10 * 1000,
                ServiceId = serviceId
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        public static bool OnlyHexInString(string test)
        {
            // For C-style hex notation (0xFF) you can use @"\A\b(0[xX])?[0-9a-fA-F]+\b\Z"
            return System.Text.RegularExpressions.Regex.IsMatch(test, @"\A\b[0-9a-fA-F]+\b\Z");
        }

        private static void AirtelVerifyTerminal()
        {
            var command = new Command
            {
                Cmd = "airtel_verify_terminal",
            };

            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void AirtelHealthCheck()
        {
            var command = new Command
            {
                Cmd = "airtel_health_check",
            };

            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void VerifyTerminal()
        {
            Console.WriteLine("Enter Terminal Id : ");
            var tid = "14034986";
            if (requestUser)
            {
                tid = Console.ReadLine();
            }
            Console.WriteLine("Enter Merchant Id : ");
            var mid = "";
            if (requestUser)
            {
                mid = Console.ReadLine();
            }

            Console.WriteLine("TID : " + tid);
            Console.WriteLine("MID : " + mid);

            var command = new Command
            {
                Cmd = "verify_terminal",
                Tid = tid
            };
            if (!string.IsNullOrWhiteSpace(mid))
            {
                command.Mid = mid;
            }

            Console.WriteLine("Going to send the data : " + JsonConvert.SerializeObject(command));
            Console.ReadLine();

            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void ServiceBlock()
        {
            Console.WriteLine("Enter 1 - To block service 1012");
            Console.WriteLine("Enter 2 - To block service 2021");
            Console.Write("Option : ");
            var addType = Console.ReadLine();
            var addTypeStr = "";

            if (addType == "1")
                addTypeStr = "1012";

            if (addType == "2")
                addTypeStr = "2021";

            if (string.IsNullOrWhiteSpace(addTypeStr))
            {
                Console.WriteLine("Invalid input");
                return;
            }

            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "service_create",
                TimeOut = 10 * 1000,
                ServiceId = addTypeStr,
                IsServiceBlock = 1
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void SetReaderTime()
        {
            var command = new Command
            {
                Cmd = "set_time",
                //Time = "20221116090000"
            };
            var time = "20221116090000";
            //if (requestUser)
            {
                Console.WriteLine("Enter the time like 20221116090000 (YYYYMMDDHHMISS : ");
                time = Console.ReadLine();
            }

            if (string.IsNullOrWhiteSpace(time))
            {
                Console.WriteLine("Invalid time");
                return;
            }
            command.Time = time;

            Console.WriteLine("Going to set the time : " + time);

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void DoBeep()
        {
            var command = new Command
            {
                Cmd = "do_beep"
            };

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void Purchase(string mode)
        {
            isPurchase = true;
            Console.WriteLine("Purchase amount to be entered without decimal '.'");
            Console.WriteLine("For example if the amount is 110.00, Please enter as 11000");
            Console.Write("Enter the purchase amount : ");
            if (autoLoopSingleInReady)
            {
                purchaseAmount = "0";
                purchaseCounter++;
                Console.WriteLine("");
                Console.WriteLine("Purchase counter : " + purchaseCounter);
            }
            else
            {
                purchaseAmount = Console.ReadLine();
            }
            //purchaseAmount = "0";
            Console.WriteLine("Purchase Amount : " + purchaseAmount);
            //Console.WriteLine($"Counter : {++counter}");
            var command = new Command
            {
                Cmd = "search_card",
                Mode = mode,
                TrxType = "purchase",
                TimeOut = searchTimeOut
            };

            if (sendAmountAtStart)
                command.Amount = purchaseAmount;

            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);

            //SendParallelPurchase(strData);
        }

        /*
        private static void SendParallelPurchase(object message)
        {
            while(true)
            {
                DateTime dt = DateTime.Now;
                if (dt.Second == 0)
                {
                    SendMessageNoReturn(message);
                    SendMessageNoReturn(message);
                    break;
                }
            }
        }
        */

        private static void SendPurchaseMessage()
        {
            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "purchase",
                TimeOut = searchTimeOut
            };

            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void PurchaseWithDateCheck()
        {
            isPurchase = true;
            Console.Write("Purchase amount defaults to 0");
            purchaseAmount = "0";
            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "purchase",
                TimeOut = searchTimeOut,
                CheckDate = "2026-12-31",
            };
            var strData = JsonConvert.SerializeObject(command);
            var result = SendMessage(strData);

            Console.WriteLine($"Result : {result}");

        }

        private static void PurchaseSingleWithData()
        {
            isPurchase = true;
            Console.Write("Enter the purchase amount : ");
            purchaseAmount = Console.ReadLine();
            Console.Write("Enter the First byte of data : ");
            dataFirstByte = Console.ReadLine();
            var command = new Command
            {
                Cmd = "search_card",
                Mode = "single",
                TrxType = "purchase",
                TimeOut = 20
            };
            var strData = JsonConvert.SerializeObject(command);
            var result = SendMessage(strData);

            Console.WriteLine($"Result : {result}");
        }

        private static void AbortSearch()
        {
            var command = new Command
            {
                Cmd = "stop"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void AbortPolling()
        {
            var command = new Command
            {
                Cmd = "abort_polling"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void GetReversal()
        {
            var command = new Command
            {
                Cmd = "get_reversal"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void DeleteFile()
        {
            var command = new Command
            {
                Cmd = "delete_file"
            };

            var fileName = "l3app.log";
            if (requestUser)
            {
                Console.WriteLine("Enter the file name to delete : ");
                fileName = Console.ReadLine();
            }

            if (string.IsNullOrWhiteSpace(fileName))
            {
                Console.WriteLine("File name cannot be empty");
                return;
            }
            command.FileName = fileName;

            var strData = JsonConvert.SerializeObject(command);
            SendDataSocketMessage(strData);
        }

        private static void ClearReversal()
        {
            var command = new Command
            {
                Cmd = "clear_reversal"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void Reboot()
        {
            var command = new Command
            {
                Cmd = "reboot"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void RequestStatus()
        {
            var command = new Command
            {
                Cmd = "status"
            };
            var strData = JsonConvert.SerializeObject(command);
            SendMessage(strData);
        }

        private static void ReceiveMessage()
        {
            byte[] bytes = new byte[1024 * 1000];

            while (true)
            {
                if (sender == null || !sender.Connected)
                {
                    Console.WriteLine("Socket not connected. Please reconnect");
                    break;
                }
                try
                {
                    // Receive the response from the remote device.
                    // Multi receive
                    /*
                    var result = "";
                    int bytesRec;
                    while (sender.Available > 0)
                    {
                        bytesRec = sender.Receive(bytes);
                        var data = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                        result += data;
                        if (bytesRec == 0)
                            break;
                    }*/

                    // Single receive
                    int bytesRec = sender.Receive(bytes);

                    var result = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                    result = ParseJson(result);

                    if (!string.IsNullOrWhiteSpace(result))
                    {
                        Console.WriteLine("-----------------------------------------");
                        Console.WriteLine($"Message Received :");
                        Console.WriteLine("COUNTER ::: " + purchaseCounter++);
                        Console.WriteLine(result);
                        Console.WriteLine("-----------------------------------------");

                        SendPurchaseAmount(result);
                        SendGateOpen(result);
                    }

                    if (result.Contains("Ready") && autoLoopSingleInReady)
                    {
                        
                        Purchase("single");
                    }

                }
                catch (Exception ex)
                {
                    isConnected = false;
                    isDataConnected = false;
                    Console.WriteLine($"Error in Receive data : {ex}");
                }

            }
        }

        private static void ReceiveDataSocketMessage()
        {
            byte[] bytes = new byte[1024 * 1000];

            while (true)
            {
                if (dataSocket == null || !dataSocket.Connected)
                {
                    Console.WriteLine("Data Socket not connected. Please reconnect");
                    break;
                }
                try
                {
                    // Receive the response from the remote device.
                    // Multi receive
                    /*
                    var result = "";
                    int bytesRec;
                    while (sender.Available > 0)
                    {
                        bytesRec = sender.Receive(bytes);
                        var data = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                        result += data;
                        if (bytesRec == 0)
                            break;
                    }*/

                    //Console.WriteLine("Message received from L3");
                    if (downloadFileMode)
                    {
                        Console.WriteLine("Download file mode");
                        int size = 0;
                        try
                        {
                            int bReceived;
                            do
                            {
                                var buffer = new byte[1024 * 10];
                                bReceived = dataSocket.Receive(buffer, 1024 * 10, SocketFlags.None);
                                Console.WriteLine("Data Socket, Download Mode : Received : " + bReceived);
                                if (bReceived >= 4)
                                {
                                    if (buffer[bReceived - 1] == 0xFC &&
                                        buffer[bReceived - 2] == 0xFD &&
                                        buffer[bReceived - 3] == 0xFE &&
                                        buffer[bReceived - 4] == 0xFF)
                                    {
                                        Console.WriteLine("Terminating data received for file");
                                        downloadFileStream.Write(buffer, 0, bReceived - 4);
                                        break;
                                    }
                                }
                                else
                                {
                                    Console.WriteLine("Writing to file");
                                    downloadFileStream.Write(buffer, 0, bReceived);
                                }
                                size += bReceived;
                            } while (bReceived > 0);

                            Console.WriteLine("Closing the stream");
                            Console.WriteLine("Total size received : " + size);
                            downloadFileStream.Close();
                            Console.WriteLine("File written to : " + fileLocalLocation);
                            downloadFileMode = false;
                            fileLocalLocation = "";
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine("Error in downloading the file : " + ex);
                        }
                    }


                    // Single receive
                    //Console.WriteLine("Data Socket : Waiting for normal receive");
                    int bytesRec = dataSocket.Receive(bytes);
                    //Console.WriteLine("Data Socket : Normal Bytes received : " + bytesRec);
                    if (downloadFileMode)
                    {
                        Console.WriteLine("Download file mode, Received : " + bytesRec);

                        if (bytesRec >= 4)
                        {
                            if (bytes[bytesRec - 1] == 0xFC &&
                                bytes[bytesRec - 2] == 0xFD &&
                                bytes[bytesRec - 3] == 0xFE &&
                                bytes[bytesRec - 4] == 0xFF)
                            {
                                Console.WriteLine("Terminating data received for file");
                                downloadFileStream.Write(bytes, 0, bytesRec - 4);
                                Console.WriteLine("Closing the stream");
                                Console.WriteLine("Total size received : " + bytesRec);
                                downloadFileStream.Close();
                                Console.WriteLine("File written to : " + fileLocalLocation);
                                downloadFileMode = false;
                                fileLocalLocation = "";
                            }
                            else
                            {
                                Console.WriteLine("Writing to file");
                                downloadFileStream.Write(bytes, 0, bytesRec);
                            }
                        }
                        else
                        {
                            Console.WriteLine("Writing to file");
                            downloadFileStream.Write(bytes, 0, bytesRec);
                        }
                    }
                    else
                    {
                        //Console.WriteLine("Data Socket String mode");
                        var result = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                        result = ParseJson(result);

                        if (!string.IsNullOrWhiteSpace(result))
                        {
                            Console.WriteLine("-----------------------------------------");
                            Console.WriteLine($"Data Socket Message Received :");
                            Console.WriteLine(result);
                            Console.WriteLine("-----------------------------------------");

                            SendPurchaseAmount(result);
                            if (result.Contains("fetch_auth") && fetchAuthLoop)
                            {
                                FetchAuth("Success");
                            }
                        }
                    }


                }
                catch (Exception ex)
                {
                    isConnected = false;
                    isDataConnected = false;
                    Console.WriteLine($"Error in Receive Data socket data : {ex}");
                }

            }
        }

        private static void SendPurchaseAmount(string messageReceived)
        {
            if (sendAmountAtStart)
                return;

            if (!isPurchase)
                return;

            if (string.IsNullOrWhiteSpace(messageReceived))
                return;

            if (messageReceived.Contains("\"state\": \"card_presented\""))
            {
                if (!isLoopMode)
                    isPurchase = false;

                var command = new Command
                {
                    Cmd = "write_card",
                    Amount = purchaseAmount
                    //ServiceData = "0001"
                };

                if (!string.IsNullOrWhiteSpace(dataFirstByte))
                {
                    command.ServiceData = GenerateServiceData();
                }

                var strData = JsonConvert.SerializeObject(command);
                Console.WriteLine(strData);
                //Thread.Sleep(500);
                SendMessage(strData);
                //SendPurchaseMessage();
            }
        }

        private static void SendGateOpen(string messageReceived)
        {
            if (string.IsNullOrWhiteSpace(messageReceived))
                return;

            if (messageReceived.Contains("\"state\": \"card_presented_abt\""))
            {
                Console.WriteLine("Card Presented ABT, Sending gate open as true");

                var command = new Command
                {
                    Cmd = "gate_open",
                    GateStatus = true
                };


                var strData = JsonConvert.SerializeObject(command);
                Console.WriteLine(strData);
                //Thread.Sleep(500);
                SendMessage(strData);
                //SendPurchaseMessage();
            }
        }

        private static string GenerateServiceData()
        {
            var data = dataFirstByte;
            for(var i = 0; i < 95; i++)
            {
                data += "00";
            }

            //return data;
            //return "abcdefa00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
            return "AB00000004801705D0050000000000000000200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
        }

        private static string ParseJson(string data)
        {
            try
            {
                return JToken.Parse(data).ToString(Formatting.Indented);
            }
            catch (Exception)
            {
                return data;
            }
        }
        /*
        private static void SendMessageNoReturn(object message)
        {
            var result = string.Empty;

            // Connect the socket to the remote endpoint. Catch any errors.    
            try
            {
                if (isSerialConnect)
                {
                    serialDataReceived = string.Empty;
                    serialPort.Write(message.ToString());
                }
                else
                {
                    //Console.WriteLine("Message to Send");
                    //Console.WriteLine(message);
                    // Encode the data string into a byte array.    
                    byte[] msg = Encoding.ASCII.GetBytes(message.ToString());

                    // Send the data through the socket.    
                    int bytesSent = sender.Send(msg);

                    // Receive the response from the remote device.    
                    //int bytesRec = sender.Receive(bytes);

                    //result = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                }

            }
            catch (ArgumentNullException ane)
            {
                Console.WriteLine("ArgumentNullException : {0}", ane.ToString());
            }
            catch (SocketException se)
            {
                Console.WriteLine("SocketException : {0}", se.ToString());
            }
            catch (Exception e)
            {
                Console.WriteLine("Unexpected exception : {0}", e.ToString());
            }
        }
        */

        private static string SendMessage(string message)
        {
            var result = string.Empty;

            // Connect the socket to the remote endpoint. Catch any errors.    
            try
            {
                if (isSerialConnect)
                {
                    serialDataReceived = string.Empty;
                    serialPort.Write(message);
                }
                else
                {
                    //Console.WriteLine("Message to Send");
                    //Console.WriteLine(message);
                    // Encode the data string into a byte array.    
                    byte[] msg = Encoding.ASCII.GetBytes(message);

                    // Send the data through the socket.    
                    int bytesSent = sender.Send(msg);

                    // Receive the response from the remote device.    
                    //int bytesRec = sender.Receive(bytes);

                    //result = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                }

            }
            catch (Exception e)
            {
                isConnected = false;
                isDataConnected = false;
                Console.WriteLine("Unexpected exception : {0}", e.ToString());
            }

            return result;
        }

        private static string SendDataSocketMessage(string message)
        {
            var result = string.Empty;
            Console.WriteLine("Sending data socket message : " + message);

            // Connect the socket to the remote endpoint. Catch any errors.    
            try
            {
                if (isSerialConnect)
                {
                    serialDataReceived = string.Empty;
                    serialPort.WriteLine(message);
                }
                else
                {
                    //Console.WriteLine("Message to Send");
                    //Console.WriteLine(message);
                    // Encode the data string into a byte array.    
                    byte[] msg = Encoding.ASCII.GetBytes(message);

                    // Send the data through the socket.    
                    int bytesSent = dataSocket.Send(msg);

                    // Receive the response from the remote device.    
                    //int bytesRec = sender.Receive(bytes);

                    //result = Encoding.ASCII.GetString(bytes, 0, bytesRec);
                }

            }
            catch (Exception e)
            {
                isConnected = false;
                isDataConnected = false;
                Console.WriteLine("Unexpected exception : {0}", e.ToString());
            }

            return result;
        }
    }
}
