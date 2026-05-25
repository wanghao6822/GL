using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using Modbus;
using Modbus.Device;

namespace RemoteIOGuide
{
    public partial class frMain : Form
    {
        /// <summary>
        /// 通信锁
        /// </summary>
        object myLock = new object();

        /// <summary>
        /// Modbus设备
        /// </summary>
        IModbusMaster myModbus;
        /// <summary>
        /// 已连接
        /// </summary>
        bool connected;
        /// <summary>
        /// 连接中
        /// </summary>
        bool connecting;
        /// <summary>
        /// 发送信息标志
        /// </summary>
        bool sendDataFlag;
        /// <summary>
        /// 显示设备信息
        /// </summary>
        bool showDeviceInfo;

        DeviceDataClass myDeviceData;

        TcpClient deviceTcp;

        List<CheckBox> inputList;
        List<CheckBox> outputList;

        // 串口参数
        byte stationNum = 1;
        string portName = "COM1";
        int baudRate = 115200;
        // 创建串口连接
        SerialPort serialPort;

        /// <summary>
        /// 读取数据任务
        /// </summary>
        Task ReadTask;



        public frMain()
        {
            InitializeComponent();
        }

        /// <summary>
        /// 显示消息
        /// </summary>
        /// <param name="msg"></param>
        private void ShowMsg(string msg)
        {
            listBox1.Invoke(new Action(() =>
            {
                if (listBox1.Items.Count >= 500)
                {
                    listBox1.Items.RemoveAt(0);
                }
                listBox1.SelectedIndex = listBox1.Items.Add(DateTime.Now.ToString("HH:mm:ss:fff") + "\t" + msg);
            }));
        }
        private void frMain_Load(object sender, EventArgs e)
        {
            myDeviceData = new DeviceDataClass();
            inputList = new List<CheckBox>
            {
                cbx0,
                cbx1,
                cbx2,
                cbx3,
                cbx4,
                cbx5,
                cbx6,
                cbx7
            };
            outputList = new List<CheckBox> {
                cbxY0,
                cbxY1,
                cbxY2,
                cbxY3,
                cbxY4,
                cbxY5,
                cbxY6,
                cbxY7
            };
            ReloadParameter();
        }


        /// <summary>
        /// 重载参数
        /// </summary>
        private void ReloadParameter()
        {
            panel12.Visible = false;
            GetComPort();
            cbxBaudRate.Items.Clear();
            cbxBaudRate.Items.AddRange(Enum.GetNames(typeof(BaudRateEnum)).Select(a => a.Remove(0, 4)).ToArray());
            cbxStationNum.SelectedIndex = 0;
            cbxBaudRate.SelectedIndex = 0;
        }
        private void GetComPort()
        {
            cbxSerialPort.Items.Clear();
            cbxSerialPort.Items.AddRange(SerialPort.GetPortNames());
        }
        private void radioButton3_CheckedChanged(object sender, EventArgs e)
        {
            panel11.Visible = true;
            panel12.Visible = false;
            GetComPort();
        }

        private void radioButton4_CheckedChanged(object sender, EventArgs e)
        {
            panel11.Visible = false;
            panel12.Visible = true;
        }
        /// <summary>
        /// 连接按钮
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void btnConnect_Click(object sender, EventArgs e)
        {
            if (connected)
            {
                MessageBox.Show("设备已连接，请先断开后再连接。", "提示", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (connecting)
            {
                MessageBox.Show("设备连接中，请稍后再试。", "提示", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            bool state = false;
            connecting = true;
            if (rbtnSerialPortSelect.Checked)
            {
                state = await SerialConnect();//串口连接
            }
            else if (rbtnEthenetSelect.Checked)
            {
                state = await EthnerNetConnectAsync();//网口连接
            }
            if (state)
            {
                btnConnect.BackColor = Color.LightGreen;
                btnConnect.Text = "已连接";
                showDeviceInfo = true;
                ReadTask = Task.Run(() => { ReadDeviceData(); });//开始循环读取任务                
                ShowMsg("连接成功");
                MessageBox.Show("连接成功。", "提示", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
            }
            else
            {
                ShowMsg("连接失败");
            }
            connecting = false;
        }

        /// <summary>
        /// 串口连接
        /// </summary>
        private async Task<bool> SerialConnect()
        {
            try
            {
                portName = cbxSerialPort.Text;
                stationNum = byte.Parse(cbxStationNum.Text);
                baudRate = int.Parse(cbxBaudRate.Text);
                if (!portName.Contains("COM"))
                {
                    throw new Exception("选择了无效端口");
                }
                serialPort = new SerialPort(portName, baudRate) { ReadTimeout = 500, WriteTimeout = 500 };
                serialPort.Open();
                myModbus = ModbusSerialMaster.CreateRtu(serialPort);
                myDeviceData.DeviceData = GetDeviceData(0, 20);
                connected = true;
                return true;
            }
            catch (Exception ex)
            {
                DisConnect();
                connected = false;
                string msg = "连接错误：" + ex.Message;
                ShowMsg(msg);
                MessageBox.Show(msg, "串口连接错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
        }
        /// <summary>
        /// 网络连接
        /// </summary>
        private async Task<bool> EthnerNetConnectAsync()
        {
            try
            {
                IPAddress ip = IPAddress.Parse(tbxIP.Text);
                deviceTcp = new TcpClient();
                //开始连接PLC，这里使用异步方法
                var t = await Task.Run(() => { return deviceTcp.ConnectAsync(ip, 502).Wait(2000)/*2S超时*/; });
                if (!t)//连接失败
                {
                    throw new Exception("设备连接超时，请检查：\r1.设备是否上电？\r2.网络连接是否正常？可通过Ping命令测试。\r3.IP地址是否正确？");
                }
                myModbus = ModbusIpMaster.CreateIp(deviceTcp);
                myDeviceData.DeviceData = GetDeviceData(0, 20);
                connected = true;
                return true;
            }
            catch (Exception ex)
            {
                DisConnect();
                string msg = "连接错误：" + ex.Message;
                ShowMsg(msg);
                MessageBox.Show(msg, "网络连接错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
        }
        /// <summary>
        /// 显示设备的信息
        /// </summary>
        private void ShowDeviceInfo()
        {
            //ShowMsg("ShowInfo");
            myDeviceData.DeviceDataUpdate();
            tbxDeviceVersion.Text = myDeviceData.Verison;
            tbxDeviceStationNum.Text = myDeviceData.StationNum.ToString();
            tbxDeviceBaudRate.Text = myDeviceData.BaudRate.ToString();
            tbxDeviceFillter.Text = myDeviceData.InputFilter.ToString();
            tbxDeviceIP.Text = myDeviceData.IPAddress.ToString();
            tbxDeviceMac.Text = string.Join(":", myDeviceData.MAC.Select(a => a.ToString("X2")));
        }
        /// <summary>
        /// 断开连接
        /// </summary>
        private void DisConnect()
        {
            if (connected)
            {
                connected = false;
                ReadTask?.Wait(800);
                ShowMsg("连接已断开");
                btnConnect.Invoke(new Action(() =>
                {
                    btnConnect.BackColor = Color.Transparent;
                    btnConnect.Text = "连接";
                }));
            }
            serialPort?.Close();
            deviceTcp?.Close();

        }
        /// <summary>
        /// 连续读取设备数据
        /// </summary>
        private async void ReadDeviceData()
        {
            try
            {
                //DateTime dateTime;
                while (connected)
                {
                    //dateTime = DateTime.Now;
                    myDeviceData.DeviceData = GetDeviceData(0, 20);
                    //TimeSpan ts = DateTime.Now - dateTime;
                    //ShowMsg(ts.TotalMilliseconds.ToString());
                    this.Invoke(new Action(() =>
                    {
                        if (showDeviceInfo)
                        {
                            showDeviceInfo = false;
                            ShowDeviceInfo();
                        }
                        IOProcessClass.ShowIOPort(inputList, myDeviceData.DeviceData[11]);
                        IOProcessClass.ShowIOPort(outputList, myDeviceData.DeviceData[12]);
                    }));
                    await Task.Delay(500);//扫描间隔500ms
                }
            }
            catch (Exception ex)
            {
                DisConnect();
                string msg = "数据读取错误：" + ex.Message;
                ShowMsg(msg);
                MessageBox.Show(msg + "\r\n将断开连接。", "警告", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// 断开连接按钮
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            DisConnect();
        }

        /// <summary>
        /// 设定IO口
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnSetOutState_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                if (!sendDataFlag)
                {
                    int port = cbxOutPortSelect.SelectedIndex;
                    Task.Run(() =>
                    {
                        TimeSpan ts;
                        DateTime dt;

                        sendDataFlag = true;
                        if (port >= 0)
                        {
                            try
                            {
                                //dt = DateTime.Now;
                                var reslut = GetDeviceData(12, 1).First();
                                //ts = DateTime.Now - dt;
                                // ShowMsg("Read:" + ts.TotalMilliseconds);
                                //先清除要设置的位置的比特位
                                reslut &= (ushort)~(1 << port);
                                //设置指定位置的比特位为指定的值
                                reslut |= (ushort)((rbtnOn.Checked ? 1 : 0) << port);
                                //dt = DateTime.Now;
                                SetDeviceData(12, reslut);
                                //ts = DateTime.Now - dt;
                                //ShowMsg("Write:" + ts.TotalMilliseconds);
                            }
                            catch (Exception ex)
                            {
                                string msg = $"设置端口错误:{ex.Message}";
                                ShowMsg(msg);
                                MessageBox.Show(msg, "警告", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            }
                        }
                        sendDataFlag = false;
                    });
                }

            }
        }
        /// <summary>
        /// 检查连接状态
        /// </summary>
        /// <returns></returns>
        private bool CheckConnect()
        {
            if (connected)
            {
                return true;
            }
            else
            {
                string msg = $"设备未连接，请先连接设备！";
                ShowMsg(msg);
                MessageBox.Show(msg, "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return false;
            }
        }
        private void frMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            DisConnect();
        }
        /// <summary>
        /// 重启按钮
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnReboot_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                if (MessageBox.Show($"确定重启？重启后需要重新连接！", "提示", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
                {
                    return;
                }
                try
                {
                    myDeviceData.SetCommand(DeviceCommand.Reboot);
                    SetDeviceData(3, myDeviceData.DeviceData[3]);//发送命令
                    showDeviceInfo = true;//再次显示信息

                    DisConnect();
                    string msg = $"设置设备重启成功，请重新建立连接！";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
                catch (Exception ex)
                {
                    string msg = $"设置错误:{ex.Message}";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "警告", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }
        /// <summary>
        /// 保存参数按钮
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnSave_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                if (MessageBox.Show($"确定修改以下信息？\r输入滤波时间：{tbxDeviceFillter.Text}ms\r" +
                    $"IP地址：{tbxDeviceIP.Text}\rMAC地址{tbxDeviceMac.Text}？", "提示", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
                {
                    return;
                }
                try
                {
                    myDeviceData.SetInputFillter(tbxDeviceFillter.Text);
                    myDeviceData.SetMAC(tbxDeviceMac.Text);
                    myDeviceData.SetIP(tbxDeviceIP.Text);
                    myDeviceData.SetCommand(DeviceCommand.Save);
                    ushort[] sendData = new ushort[7];
                    Array.Copy(myDeviceData.DeviceData, 3, sendData, 0, 7);
                    SetDeviceData(3, sendData);//发送命令
                    showDeviceInfo = true;//再次显示信息

                    string msg = $"保存成功！";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
                catch (Exception ex)
                {
                    string msg = $"设置错误:{ex.Message}";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "警告", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }
        /// <summary>
        /// 重载参数
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnReload_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                if(MessageBox.Show($"确定重载参数？", "提示", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
                {
                    return;
                }
                try
                {

                    myDeviceData.SetCommand(DeviceCommand.Load);
                    SetDeviceData(3, myDeviceData.DeviceData[3]);//发送命令
                    showDeviceInfo = true;//再次显示信息
                    string msg = $"重载参数成功！";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
                catch (Exception ex)
                {
                    string msg = $"设置错误:{ex.Message}";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "警告", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }
        /// <summary>
        /// 恢复出厂设置
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnRestoreFactory_Click(object sender, EventArgs e)
        {
            if (CheckConnect())
            {
                if (MessageBox.Show("确定恢复出厂设置？\r出厂参数：\r输入滤波时间：5ms\rIP：192.168.1.168\rMAC:由MCUI自动生成！", "提示", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
                {
                    return;
                }
                try
                {
                    myDeviceData.SetCommand(DeviceCommand.ReFactory);
                    SetDeviceData(3, myDeviceData.DeviceData[3]);//发送命令
                    showDeviceInfo = true;//再次显示信息

                    DisConnect();
                    string msg = $"恢复出厂设置成功，请重新连接！";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
                catch (Exception ex)
                {
                    string msg = $"设置错误:{ex.Message}";
                    ShowMsg(msg);
                    MessageBox.Show(msg, "警告", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        /// <summary>
        /// 获取设备数据
        /// </summary>
        /// <param name="dataAdd"></param>
        /// <returns></returns>
        private ushort[] GetDeviceData(ushort dataAdd, ushort length)
        {
            lock (myLock)
            {
                return myModbus.ReadHoldingRegisters(stationNum, dataAdd, length);
            }

        }
        /// <summary>
        /// 设置多个数据
        /// </summary>
        /// <param name="dataAdd"></param>
        /// <param name="data"></param>

        private void SetDeviceData(ushort dataAdd, ushort[] data)
        {
            lock (myLock)
            {
                myModbus.WriteMultipleRegisters(stationNum, dataAdd, data);
            }

        }
        /// <summary>
        /// 设置单个数据
        /// </summary>
        /// <param name="dataAdd"></param>
        /// <param name="data"></param>
        private void SetDeviceData(ushort dataAdd, ushort data)
        {
            lock (myLock)
            {
                myModbus.WriteSingleRegister(stationNum, dataAdd, data);
            }

        }

        private void toolStripMenuItem1_Click(object sender, EventArgs e)
        {
            new frVersion().ShowDialog();
        }

        private void 帮助HToolStripMenuItem_Click(object sender, EventArgs e)
        {
            string path = Application.StartupPath + "\\HelpFile\\操作流程.pdf";

            try
            {
                // 打开 PDF 文件
                Process.Start(path);
            }
            catch (Exception ex)
            {
                // 处理异常（例如文件不存在或没有默认的 PDF 查看器）
                MessageBox.Show("无法打开 PDF 文件：" + ex.Message);
            }
        }
    }
}
