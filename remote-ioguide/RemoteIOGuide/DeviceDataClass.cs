using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace RemoteIOGuide
{
    public enum DeviceCommand
    {
        /// <summary>
        /// 保存
        /// </summary>
        Save = 10,
        /// <summary>
        /// 重载
        /// </summary>
        Load = 20,
        /// <summary>
        /// 重启
        /// </summary>
        Reboot = 30,
        /// <summary>
        /// 恢复出厂设置
        /// </summary>
        ReFactory = 66
    }
    /// <summary>
    /// 波特率
    /// </summary>
    enum BaudRateEnum
    {
        Baud115200,
        Baud9600,
        Baud19200,
        Baud38400
    }
    /// <summary>
    /// 从站数据
    /// </summary>
    public class DeviceDataClass
    {
        /// <summary>
        /// 版本号
        /// </summary>
        public string Verison { get; set; }
        /// <summary>
        /// 站号
        /// </summary>
        public int StationNum { get; set; }
        /// <summary>
        /// 波特率
        /// </summary>
        public int BaudRate { get; set; }
        /// <summary>
        /// 输入滤波时间
        /// </summary>
        public int InputFilter { get; set; }
        /// <summary>
        /// MAC地址
        /// </summary>
        public byte[] MAC { get; set; }
        /// <summary>
        /// IP地址
        /// </summary>
        public IPAddress IPAddress { get; set; }
        /// <summary>
        /// 读取数据
        /// </summary>
        public ushort[] DeviceData { get; set; }

        /// <summary>
        /// 更新数据
        /// </summary>
        public void DeviceDataUpdate()
        {
            //设置版本号
            Verison = DeviceData[0].ToString();
            //设置站号
            StationNum = DeviceData[1];
            //设置波特率
            switch (DeviceData[2])
            {
                case 0:
                    BaudRate = int.Parse(BaudRateEnum.Baud115200.ToString().Remove(0, 4));
                    break;
                case 1:
                    BaudRate = int.Parse(BaudRateEnum.Baud9600.ToString().Remove(0, 4));
                    break;
                case 2:
                    BaudRate = int.Parse(BaudRateEnum.Baud19200.ToString().Remove(0, 4));
                    break;
                case 3:
                    BaudRate = int.Parse(BaudRateEnum.Baud38400.ToString().Remove(0, 4));
                    break;
                default:
                    BaudRate = int.Parse(BaudRateEnum.Baud115200.ToString().Remove(0, 4));
                    break;
            }
            //设置滤波时间
            InputFilter = DeviceData[4];
            //MAC获取

            MAC = new byte[] { (byte)(DeviceData[7] >> 8), ((byte)DeviceData[7]),
                (byte)(DeviceData[6] >> 8),((byte)DeviceData[6]),
                (byte)(DeviceData[5] >> 8),((byte)DeviceData[5])};
            ;
            //IP获取
            var IP1 = BitConverter.GetBytes(DeviceData[9]).Select((a) => { return a.ToString(); }).ToArray();
            var IP2 = BitConverter.GetBytes(DeviceData[8]).Select((a) => { return a.ToString(); }).ToArray();
            IPAddress = IPAddress.Parse($"{IP1[1]}.{IP1[0]}.{IP2[1]}.{IP2[0]}");
        }
        /// <summary>
        /// 设置命令
        /// </summary>
        /// <param name="command"></param>
        public void SetCommand(DeviceCommand command)
        {
            DeviceData[3] = (ushort)command;
        }

        /// <summary>
        /// 设置滤波时间
        /// </summary>
        /// <param name="fillterTime"></param>
        public void SetInputFillter(string fillterTime)
        {
            DeviceData[4] = ushort.Parse(fillterTime);
        }
        /// <summary>
        /// 设置MAC地址
        /// </summary>
        /// <param name="mac"></param>
        /// <exception cref="Exception"></exception>
        public void SetMAC(string mac)
        {
            var newMAC = mac.Split(':');
            if (newMAC.Length != 6)
            {
                throw new Exception("MAC地址不是6组数据");
            }
            List<ushort> data = new List<ushort>();
            for (int i = 0; i < newMAC.Length; i++)
            {
                data.Add(Convert.ToUInt16(newMAC[i], 16));
            }
            DeviceData[7] = (ushort)((data[0] << 8) + data[1]);
            DeviceData[6] = (ushort)((data[2] << 8) + data[3]);
            DeviceData[5] = (ushort)((data[4] << 8) + data[5]);
        }

        /// <summary>
        /// 设置IP地址
        /// </summary>
        /// <param name="ip"></param>
        public void SetIP(string ip)
        {
            var newIP = ip.Split('.');
            if (newIP.Length != 4)
            {
                throw new Exception("IP地址不是4组数据");
            }
            List<ushort> data = new List<ushort>();
            for (int i = 0; i < newIP.Length; i++)
            {
                data.Add(Convert.ToUInt16(newIP[i]));
            }
            DeviceData[9] = (ushort)((data[0] << 8) + data[1]);
            DeviceData[8] = (ushort)((data[2] << 8) + data[3]);

        }
    }
}
