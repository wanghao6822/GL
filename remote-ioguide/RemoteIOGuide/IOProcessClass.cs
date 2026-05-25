using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace RemoteIOGuide
{
    /// <summary>
    /// IO处理类
    /// </summary>
    public static class IOProcessClass
    {
        public static void ShowIOPort(List<CheckBox> cbxList, ushort data)
        {
            for (int i = 0; i < cbxList.Count; i++)
            {
                if (i > 15)
                {
                    break;
                }
                cbxList[i].Checked = Convert.ToBoolean(((data >> i) & 0x01));
            }
        }
        public static ushort GetIOPort(List<CheckBox> cbxList)
        {
            ushort data = 0;
            for (int i = 0; i < cbxList.Count; i++)
            {
                if (i > 15)
                {
                    break;
                }
                if (cbxList[i].Checked)
                {
                    data |= (ushort)(1 << i);
                }
            }
            return data;
        }
    }

}
