using System;
using Newtonsoft.Json;
using Simulator;

namespace SimulatorApp
{
    public class Command
    {
        [JsonProperty("cmd")]
        public string Cmd { get; set; }

        [JsonProperty("time")]
        public string Time { get; set; }

        [JsonProperty("mode")]
        public string Mode { get; set; }

        [JsonProperty("trxtype")]
        public string TrxType { get; set; }

        [JsonProperty("timeout")]
        public int TimeOut { get; set; }

        [JsonProperty("amount")]
        public string Amount { get; set; }

        [JsonProperty("maxrecords")]
        public int MaxRecords { get; set; }

        [JsonProperty("fetchid")]
        public int FetchId { get; set; }

        [JsonProperty("servicedata")]
        public string ServiceData { get; set; }

        [JsonProperty("config")]
        public Config config;

        [JsonProperty("money_add_type")]
        public string MoneyAddType { get; set; }

        [JsonProperty("money_add_source_txn")]
        public string MoneyAddSourceTxn { get; set; }

        [JsonProperty("service_id")]
        public string ServiceId { get; set; }

        [JsonProperty("is_service_block")]
        public int IsServiceBlock { get; set; }

        [JsonProperty("check_date")]
        public string CheckDate { get; set; }

        [JsonProperty("tid")]
        public string Tid { get; set; }

        [JsonProperty("mid")]
        public string Mid { get; set; }

        [JsonProperty("ip_mode")]
        public string IpMode { get; set; }

        [JsonProperty("dns")]
        public string Dns { get; set; }

        [JsonProperty("dns2")]
        public string Dns2 { get; set; }

        [JsonProperty("dns3")]
        public string Dns3 { get; set; }

        [JsonProperty("dns4")]
        public string Dns4 { get; set; }

        [JsonProperty("search_domain")]
        public string SearchDomain { get; set; }

        [JsonProperty("ip_address")]
        public string IpAddress { get; set; }

        [JsonProperty("netmask")]
        public string Netmask { get; set; }

        [JsonProperty("gateway")]
        public string Gateway { get; set; }

        [JsonProperty("log_mode")]
        public string LogMode { get; set; }

        [JsonProperty("label")]
        public string Label { get; set; }

        [JsonProperty("isBdk")]
        public bool IsBdk { get; set; }

        [JsonProperty("download_file_name")]
        public string DownloadFileName { get; set; }

        [JsonProperty("delete_file_name")]
        public string FileName { get; set; }

        [JsonProperty("gate_status")]
        public bool GateStatus { get; set; }

        [JsonProperty("skipRecords")]
        public int SkipRecords { get; set; }
    }

    public class Message
    {
        public string messageId { get; set; }
        public string messageType { get; set; }
        public string target { get; set; }
        public string source { get; set; }
        public Data data { get; set; }
    }

    public class Data
    {
        public string fileName { get; set; }
        public string ip_mode { get; set; }
        public string dns { get; set; }
        public string ip_address { get; set; }
        public string netmask { get; set; }
        public string gateway { get; set; }

        public string sourceFile { get; set; }
        public string destFile { get; set; }
    }
}
