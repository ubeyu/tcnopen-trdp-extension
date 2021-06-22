# 列车通信网络攻击检测仿真测试验证平台软件实现

### 本平台建设需求来源于实验室项目《车载网管交换机网络安全防御模块研发》，仓库代码主要用于列车通信网络仿真平台的软件实现。《车载网管交换机网络安全防御模块研发》项目针对列车通信网络特点，计划搭建列车通信网络攻击检测仿真测试验证平台，具备TRDP协议仿真能力和攻击注入能力，便于入侵检测数据获取，展示攻击效果。

#### 列车仿真实验平台系统主要包括平台主控端和被控端，结构框架图结构如图所示，平台采用实时以太网，三节车厢结构，包含主控模块EGWM×1、自动控制系统ATC×1、乘客信息系统PIS×1、门控系统EDCU×6、空调系统HVAC×3、制动系统BCU×3、牵引系统DCU×1、辅助逆变器SIV×2，全部采用树莓派4B模拟设备。

<table>
    <tr>
        <td><center><img src="https://github.com/wanghaoyang949/notebook-cs/blob/master/images/4.%E8%BD%A6%E8%BD%BD%E7%BD%91%E7%AE%A1%E4%BA%A4%E6%8D%A2%E6%9C%BA/%E4%BB%BF%E7%9C%9F%E5%B9%B3%E5%8F%B0%E7%BB%93%E6%9E%84%E6%A1%86%E5%9B%BE.png"></center></td>
    </tr>
</table>

<table>
    <tr>
        <td><center><img src="https://github.com/wanghaoyang949/notebook-cs/blob/master/images/4.%E8%BD%A6%E8%BD%BD%E7%BD%91%E7%AE%A1%E4%BA%A4%E6%8D%A2%E6%9C%BA/%E4%BB%BF%E7%9C%9F%E5%B9%B3%E5%8F%B0%E5%AE%9E%E4%BD%93%E7%BB%93%E6%9E%84.png"></center></td>
    </tr>
</table>

## 平台搭建的流程：
* 1.协议模拟
* 2.通信实现
* 3.可视化
* 4.攻击实现
* 5.攻击检测 

### 项目进展
#### 1.2020/12/30 空调系统和门控系统的协议添加完毕；
#### 2.2021/03/21 所有设备的通信协议结构体定义完毕。
#### 3.2021/05/21 实现仿真平台TRDP协议通信；
#### 4.2021/06/30 实现仿真平台可视化界面设计；



## TCNOpen TRDP prototype stack

*******************************************************************************************************

For further information:

* 'TCN-TRDP2-D-BOM-019-10 TRDP SYSTEM ARCHITECTURE & DESIGN SPECIFICATION'(https://www.cooperationtool.eu/tcnopen/goto.aspx?p=TCNOPEN&doc=f32ef583-5601-43dd-b1df-d5a8f309ffab)

* 'TCN-TRDP2-D-BOM-011-31 TRDP USER'S MANUAL'(https://www.cooperationtool.eu/tcnopen/goto.aspx?p=TCNOPEN&doc=e8a3340b-249b-49b6-b39a-41e780787c0d)

*******************************************************************************************************
