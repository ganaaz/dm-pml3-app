using System;
using System.Collections.Generic;

namespace Simulator
{
    public class Psp
    {
        public int port { get; set; }
        public string ip { get; set; }
        public string terminalId { get; set; }
        public string merchantId { get; set; }
        public string clientId { get; set; }
        public string clientName { get; set; }
        public string hostVersion { get; set; }
        public int txnTimeOut { get; set; }
        public int maxRetry { get; set; }
        public int hostProcessTimeInMinutes { get; set; }
        public int reversalTimeInMinutes { get; set; }
        public string httpsHostName { get; set; }
        public string offlineUrl { get; set; }
        public string serviceCreationUrl { get; set; }
        public string moneyLoadUrl { get; set; }
        public string balanceUpdateUrl { get; set; }
        public string verifyTerminalUrl { get; set; }
        public string reversalUrl { get; set; }
    }

    public class General
    {
        public int nsec_awaiting_write { get; set; }
        public string kldIp { get; set; }
        public int maxKeyInjectionTry { get; set; }
        public int keyInjectRetryDelaySec { get; set; }
        public string panTokenKey { get; set; }
        public bool beepOnCardFound { get; set; }
        public string purchaseLimit { get; set; }
        public string moneyAddLimit { get; set; }
        public string deviceCode { get; set; }
        public string equipmentType { get; set; }
        public string equipmentCode { get; set; }
        public string stationId { get; set; }
        public string stationName { get; set; }
        public int paytmLogCount { get; set; }
        public int paytmMaxLogSizeKb { get; set; }
    }

    public class Config
    {
        public Psp psp { get; set; }
        public General general { get; set; }
        public List<KeyData> keys { get; set; }
        public List<object> ledConfigs { get; set; }
    }

    public class KeyData
    {
        public string label { get; set; }
        public int slot { get; set; }
        public int mkVersion { get; set; }
        public string astId { get; set; }
        public string pkcsId { get; set; }
        public string type { get; set; }
        public string keySetIdentifier { get; set; }
        public bool isMac { get; set; }
    }

    public class L1
    {
        public List<string> waiting_key_injection { get; set; }
    }

    public class L2
    {
        public List<string> key_inject { get; set; }
    }

    public class L3
    {
        public List<string> awating_card_success { get; set; }
    }

    public class L4
    {
        public List<string> awaiting_card_failure { get; set; }
    }

    public class L5
    {
        public List<string> card_read_failed { get; set; }
    }

    public class L6
    {
        public List<string> card_presented { get; set; }
    }

    public class L7
    {
        public List<string> write_success { get; set; }
    }

    public class L8
    {
        public List<string> write_failed { get; set; }
    }

    public class L9
    {
        public List<string> card_processed_success { get; set; }
    }

    public class L10
    {
        public List<string> card_processed_failure { get; set; }
    }

    public class L11
    {
        public List<string> card_processed_success_msg_sent { get; set; }
    }

    public class L12
    {
        public List<string> app_started { get; set; }
    }

    public class L13
    {
        public List<string> app_exiting { get; set; }
    }

    public class LedConfigs
    {
        public LedItem waiting_key_injection { get; set; }
        public LedItem key_inject { get; set; }
        public LedItem awating_card_success { get; set; }
        public LedItem awaiting_card_failure { get; set; }
        public LedItem card_read_failed { get; set; }
        public LedItem card_presented { get; set; }

    }

    public class LedItem
    {
        public List<string> ledValues { get; set; }
    }
}
