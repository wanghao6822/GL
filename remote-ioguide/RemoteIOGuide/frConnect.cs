using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

namespace RemoteIOGuide
{
    public partial class frConnect : Form
    {

        Timer t;
        public frConnect()
        {
            InitializeComponent();
        }

        private void frConnect_Load(object sender, EventArgs e)
        {
            t = new Timer();
            t.Tick += T_Tick;
            t.Interval = 100;
            t.Start();
        }

        private void T_Tick(object sender, EventArgs e)
        {
            if (progressBar1.Value < progressBar1.Maximum)
            {
                progressBar1.Value += 5;
            }
            else
            {
                this.Close();
            }
            
        }

        private void frConnect_FormClosing(object sender, FormClosingEventArgs e)
        {
            t.Stop();
        }
    }
}
